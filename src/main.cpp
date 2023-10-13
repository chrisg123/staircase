#include "AppContext.hpp"
#include "EmbeddedStepFile.hpp"
#include "GraphicsUtilities.hpp"
#include "OCCTUtilities.hpp"
#include "staircase.hpp"
#include <any>
#include <chrono>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <opencascade/Standard_Version.hxx>
#include <optional>
#include <queue>
#include <thread>

bool arePthreadsEnabled() {
#ifdef __EMSCRIPTEN_PTHREADS__
  return true;
#else
  return false;
#endif
}
using DocHandle = Handle(TDocStd_Document);
void main_loop(void *arg);
void handleMessages(AppContext &context,
                    std::queue<Staircase::Message> messageQueue);
void draw(AppContext &context);
EM_BOOL onMouseCallback(int eventType, EmscriptenMouseEvent const *mouseEvent,
                        void *userData);
void initializeUserInteractions(AppContext &context);


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

  handleMessages(*context, context->drainMessageQueue());

  draw(*context);
}

void handleMessages(AppContext &context,
                    std::queue<Staircase::Message> messageQueue) {
  while (!messageQueue.empty()) {
    auto message = messageQueue.front();
    messageQueue.pop();
    std::cout << "[RCV] " << MessageType::toString(message.type) << std::endl;

    switch (message.type) {
    case MessageType::ClearScreen:
      context.renderingMode = RenderingMode::None;
      clearCanvas(Colors::Black);
      break;
    case MessageType::SetVersionString:
      EM_ASM(
          { document.getElementById('version').innerHTML = UTF8ToString($0); },
          std::any_cast<std::string>(message.data).c_str());
      break;
    case MessageType::DrawCheckerboard:
      context.renderingMode = RenderingMode::DrawCheckerboard;
      break;
    case MessageType::DrawLoadingScreen:
      context.renderingMode = RenderingMode::DrawLoadingScreen;
      break;
    case MessageType::ReadStepFile: {
      std::string stepFileStr =
          std::any_cast<std::string>(message.data).c_str();
      context.renderingMode = RenderingMode::DrawLoadingScreen;
      EM_ASM(
          { document.getElementById('stepText').innerHTML = UTF8ToString($0); },
          stepFileStr.c_str());

      std::thread([&context, stepFileStr]() {
        readStepFile(
            XCAFApp_Application::GetApplication(), stepFileStr, [&context](std::optional<DocHandle> docOpt) {
              if (!docOpt.has_value()) {
                std::cerr << "Failed to read STEP file: DocHandle is empty"
                          << std::endl;
                return;
              }

              Staircase::Message msg;
              msg.type = MessageType::ClearScreen;
              msg.callback = [docOpt, &context]() {
                context.pushMessage(Staircase::Message{
                    MessageType::RenderStepFile, docOpt.value(), nullptr});
              };

              context.pushMessage(msg);
            });
      }).detach();
      break;
    }
    case MessageType::RenderStepFile:
      DocHandle aDoc = std::any_cast<DocHandle>(message.data);
      context.currentlyViewingDoc = aDoc;
      context.renderingMode = RenderingMode::RenderStepFile;
      printLabels(aDoc->Main());
      break;
    }

    if (message.callback != nullptr) { message.callback(); }
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
  case RenderingMode::DrawCheckerboard: drawCheckerBoard(context.shaderProgram); break;
  case RenderingMode::RenderStepFile:

    if (!context.occtComponentsInitialized) {
      initializeOcctComponents(context);
      context.occtComponentsInitialized = true;
    }

    renderStepFile(context);
    break;
  case RenderingMode::DrawLoadingScreen: {
    drawLoadingScreen(context.shaderProgram, context.spinnerParams);
    break;
  }
  default: break;
  }
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
