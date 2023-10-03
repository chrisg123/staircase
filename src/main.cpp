#include "main.hpp"
#include "AppContext.hpp"
#include "EmbeddedStepFile.hpp"
#include "OCCTUtilities.hpp"
#include "staircase.hpp"
#include <AIS_InteractiveContext.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <GLES2/gl2.h>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <Wasm_Window.hxx>
#include <algorithm>
#include <any>
#include <chrono>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <opencascade/AIS_Shape.hxx>
#include <opencascade/Standard_Version.hxx>
#include <opencascade/TDF_ChildIterator.hxx>
#include <opencascade/TDataStd_Name.hxx>
#include <optional>
#include <queue>
#include <thread>

using DocHandle = Handle(TDocStd_Document);
using CallbackType = std::function<void(std::optional<DocHandle>)>;

int main() {
  AppContext context;
  std::string occt_ver_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);
  std::cout << occt_ver_str << std::endl;

  if (!arePthreadsEnabled()) {
    std::cerr << "Pthreads are required." << std::endl;
    return 1;
  }
  std::cout << "Pthreads enabled" << std::endl;

  std::string containerId = "staircase-container";
  context.canvasId = "staircase-canvas";

  createCanvas(containerId, context.canvasId);
  initializeUserInteractions(context);
  setupWebGLContext(context.canvasId);
  setupViewport(context);

  std::cout << "Canvas size: " << context.canvasWidth << "x"
            << context.canvasHeight << std::endl;

  context.shaderProgram = createShaderProgram(
      /*vertexShaderSource=*/
      "attribute vec3 position;"
      "void main() {"
      "  gl_Position  = vec4(position, 1.0);"
      "}",
      /*fragmentShaderSource=*/
      "precision mediump float;"
      "uniform vec4 color;"
      "void main() {"
      "  gl_FragColor = color;"
      "}");

  context.pushMessage(
      {MessageType::SetVersionString, std::any{occt_ver_str}, nullptr});
  context.pushMessage({MessageType::DrawCheckerboard, std::any{}, nullptr});

  std::thread([&]() {
    // Delay the STEP file loading to briefly show the initial checkerboard
    // state.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    context.pushMessage(
        {MessageType::ReadStepFile, std::any{embeddedStepFile}, nullptr});
  }).detach();

  emscripten_set_main_loop_arg(main_loop, &context, 0, 1);

  return 0;
}

void main_loop(void *arg) {

  AppContext *context = static_cast<AppContext *>(arg);

  auto localQueue = context->drainMessageQueue();

  while (!localQueue.empty()) {
    auto message = localQueue.front();
    localQueue.pop();
    std::cout << "[RCV] " << MessageType::toString(message.type) << std::endl;

    switch (message.type) {
    case MessageType::ClearScreen:
      context->renderingMode = RenderingMode::None;
      clearCanvas(Colors::Black);
      break;
    case MessageType::SetVersionString:
      EM_ASM(
          { document.getElementById('version').innerHTML = UTF8ToString($0); },
          std::any_cast<std::string>(message.data).c_str());
      break;
    case MessageType::DrawCheckerboard:
      context->renderingMode = RenderingMode::DrawCheckerboard;
      break;
    case MessageType::DrawLoadingScreen:
      context->renderingMode = RenderingMode::DrawLoadingScreen;
      break;
    case MessageType::ReadStepFile: {
      std::string stepFileStr =
          std::any_cast<std::string>(message.data).c_str();
      context->renderingMode = RenderingMode::DrawLoadingScreen;
      EM_ASM(
          { document.getElementById('stepText').innerHTML = UTF8ToString($0); },
          stepFileStr.c_str());

      std::thread([&, context]() {
        readStepFile(
            *context, stepFileStr, [context](std::optional<DocHandle> docOpt) {
              if (!docOpt.has_value()) {
                std::cerr << "Failed to read STEP file: DocHandle is empty"
                          << std::endl;
                return;
              }

              Staircase::Message msg;
              msg.type = MessageType::ClearScreen;
              msg.callback = [docOpt, context]() {
                context->pushMessage(Staircase::Message{
                    MessageType::RenderStepFile, docOpt.value(), nullptr});
              };

              context->pushMessage(msg);
            });
      }).detach();
      break;
    }
    case MessageType::RenderStepFile:
      DocHandle aDoc = std::any_cast<DocHandle>(message.data);
      context->currentlyViewingDoc = aDoc;
      context->renderingMode = RenderingMode::RenderStepFile;
      printLabels(aDoc->Main());
      break;
    }

    if (message.callback != nullptr) { message.callback(); }
  }

  draw(*context);
}

void createCanvas(std::string containerId, std::string canvasId) {
  EM_ASM(
      {
        var divElement = document.getElementById(UTF8ToString($0));

        if (divElement) {
          var canvas = document.createElement('canvas');
          canvas.id = UTF8ToString($1);
          divElement.appendChild(canvas);

          // Align canvas size with device pixels
          var computedStyle = window.getComputedStyle(canvas);
          var cssWidth = parseInt(computedStyle.getPropertyValue('width'), 10);
          var cssHeight =
              parseInt(computedStyle.getPropertyValue('height'), 10);
          var devicePixelRatio = window.devicePixelRatio || 1;
          canvas.width = cssWidth * devicePixelRatio;
          canvas.height = cssHeight * devicePixelRatio;
        }
        // Set the canvas to emscripten Module object
        Module['canvas'] = canvas;
      },
      containerId.c_str(), canvasId.c_str());
}

EM_BOOL onMouseCallback(int eventType, EmscriptenMouseEvent const *mouseEvent,
                        void *userData) {
  AppContext *context = static_cast<AppContext *>(userData);

  switch (eventType) {
  case EMSCRIPTEN_EVENT_MOUSEDOWN:
    std::cout << "[EVT] EMSCRIPTEN_EVENT_MOUSEDOWN" << std::endl;
    context->shouldRotate = !context->shouldRotate;
    break;
  default:
    std::cout << "Unhandled mouse event type: " << eventType << std::endl;
    break;
  }
  return EM_TRUE;
}

void initializeUserInteractions(AppContext &context) {
  const EM_BOOL toUseCapture = EM_TRUE;

  emscripten_set_mousedown_callback(("#" + context.canvasId).c_str(), &context,
                                    toUseCapture, onMouseCallback);
}

void renderStepFile(AppContext &context) {
  DocHandle aDoc = context.currentlyViewingDoc;
  Handle(AIS_InteractiveContext) aisContext = context.aisContext;
  Handle(V3d_View) aView = context.view;
  if (aDoc.IsNull() || context.aisContext.IsNull()) {
    std::cerr << "No document or AIS context available to render." << std::endl;
    return;
  }

  if (!context.stepFileLoaded) {
    std::vector<TopoDS_Shape> shapes = getShapesFromDoc(aDoc);
    {
      Timer timer = Timer("Populate aisContext with shapes;");
      for (auto const &shape : shapes) {
        Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
        aisContext->SetDisplayMode(aisShape, AIS_SHADED_MODE, Standard_True);
        aisContext->Display(aisShape, Standard_True);
      }
    }

    context.stepFileLoaded = true;
  }

  if (context.shouldRotate) {
    static double angle = 0.0;
    angle += 0.01;
    if (angle >= 2 * M_PI) { angle = 0.0; }

    gp_Dir aDir(sin(angle), cos(angle), aView->Camera()->Direction().Z());
    aView->Camera()->SetDirection(aDir);
    aView->Redraw();
  }
}

void draw(AppContext &context) {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "OpenGL error: " << err << std::endl;
    return;
  }
  if (!context.occtComponentsInitialized) { clearCanvas(Colors::Platinum); }

  switch (context.renderingMode) {
  case RenderingMode::ClearScreen: clearCanvas(Colors::Black); break;
  case RenderingMode::DrawCheckerboard: drawCheckerBoard(context); break;
  case RenderingMode::RenderStepFile:

    if (!context.occtComponentsInitialized) {
      initializeOcctComponents(context);
      context.occtComponentsInitialized = true;
    }

    renderStepFile(context);
    break;
  case RenderingMode::DrawLoadingScreen: {
    drawLoadingScreen(context, context.spinnerParams);
    break;
  }
  default: break;
  }
}
