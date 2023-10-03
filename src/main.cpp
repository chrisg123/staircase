#include "staircase.hpp"
#include "AppContext.hpp"
#include "main.hpp"
#include "EmbeddedStepFile.hpp"
#include <AIS_InteractiveContext.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <GLES2/gl2.h>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <Wasm_Window.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <algorithm>
#include <any>
#include <chrono>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <opencascade/AIS_Shape.hxx>
#include <opencascade/STEPCAFControl_Reader.hxx>
#include <opencascade/Standard_Version.hxx>
#include <opencascade/TDF_ChildIterator.hxx>
#include <opencascade/TDataStd_Name.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFDoc_ShapeTool.hxx>
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

void initializeOcctComponents(AppContext &context) {

  float myDevicePixelRatio = emscripten_get_device_pixel_ratio();
  Handle(Aspect_DisplayConnection) aDisp;
  Handle(OpenGl_GraphicDriver) aDriver = new OpenGl_GraphicDriver(aDisp, false);
  aDriver->ChangeOptions().buffersNoSwap = true;
  aDriver->ChangeOptions().buffersOpaqueAlpha = true;

  if (!aDriver->InitContext()) {
    std::cerr << "Error: EGL initialization failed" << std::endl;
    return;
  }

  Handle(V3d_Viewer) aViewer = new V3d_Viewer(aDriver);
  aViewer->SetComputedMode(false);
  aViewer->SetDefaultShadingModel(Graphic3d_TypeOfShadingModel_Phong);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  for (V3d_ListOfLight::Iterator aLightIter(aViewer->ActiveLights());
       aLightIter.More(); aLightIter.Next()) {
    const Handle(V3d_Light) &aLight = aLightIter.Value();
    if (aLight->Type() == Graphic3d_TypeOfLightSource_Directional) {
      aLight->SetCastShadows(true);
    }
  }

  Handle(Wasm_Window) aWindow = new Wasm_Window(context.canvasId.c_str());
  aWindow->Size(context.canvasWidth, context.canvasHeight);

  Handle(Prs3d_TextAspect) myTextStyle = new Prs3d_TextAspect();
  myTextStyle->SetFont(Font_NOF_ASCII_MONO);
  myTextStyle->SetHeight(12);
  myTextStyle->Aspect()->SetColor(Quantity_NOC_GRAY95);
  myTextStyle->Aspect()->SetColorSubTitle(Quantity_NOC_BLACK);
  myTextStyle->Aspect()->SetDisplayType(Aspect_TODT_SHADOW);
  myTextStyle->Aspect()->SetTextFontAspect(Font_FA_Bold);
  myTextStyle->Aspect()->SetTextZoomable(false);
  myTextStyle->SetHorizontalJustification(Graphic3d_HTA_LEFT);
  myTextStyle->SetVerticalJustification(Graphic3d_VTA_BOTTOM);

  Handle(V3d_View) aView = aViewer->CreateView();
  aView->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
  aView->SetImmediateUpdate(false);
  aView->ChangeRenderingParams().IsShadowEnabled = false;
  aView->ChangeRenderingParams().Resolution =
      (unsigned int)(96.0 * myDevicePixelRatio + 0.5);
  aView->ChangeRenderingParams().ToShowStats = true;
  aView->ChangeRenderingParams().StatsTextAspect = myTextStyle->Aspect();
  aView->ChangeRenderingParams().StatsTextHeight = (int)myTextStyle->Height();
  aView->SetWindow(aWindow);

  Handle(AIS_InteractiveContext) aContext = new AIS_InteractiveContext(aViewer);

  context.view = aView;
  context.viewer = aViewer;
  context.aisContext = aContext;
}

std::optional<DocHandle> readInto(std::function<DocHandle()> aNewDoc,
                                  std::istream &fromStream) {

  DocHandle aDoc = aNewDoc();
  STEPCAFControl_Reader aStepReader;

  IFSelect_ReturnStatus aStatus =
      aStepReader.ReadStream("Embedded STEP Data", fromStream);

  if (aStatus != IFSelect_RetDone) {
    std::cerr << "Error reading STEP file." << std::endl;
    return std::nullopt;
  }

  bool success = aStepReader.Transfer(aDoc);

  if (!success) {
    std::cerr << "Transfer failed." << std::endl;
    return std::nullopt;
  }

  return aDoc;
}

/**
 * Recursively prints the hierarchy of labels from a TDF_Label tree.
 *
 * @param label The root label to start the traversal from.
 * @param level Indentation level for better readability (default is 0).
 */
void printLabels(TDF_Label const &label, int level) {
  for (int i = 0; i < level; ++i) {
    std::cout << "  ";
  }

  Handle(TDataStd_Name) name;
  if (label.FindAttribute(TDataStd_Name::GetID(), name)) {
    TCollection_ExtendedString nameStr = name->Get();
    std::cout << "Label: " << nameStr;
  } else {
    std::cout << "Unnamed Label";
  }
  std::cout << std::endl;

  for (TDF_ChildIterator it(label); it.More(); it.Next()) {
    printLabels(it.Value(), level + 1);
  }
}

void readStepFile(AppContext &context, std::string stepFileStr,
                  CallbackType callback) {

  auto aNewDoc = [&]() -> DocHandle {
    Handle(TDocStd_Document) aDoc;
    context.getApp()->NewDocument("MDTV-XCAF", aDoc);
    return aDoc;
  };

  std::istringstream fromStream(stepFileStr);

  std::optional<DocHandle> docOpt;
  {
    Timer timer = Timer("readInto(aNewDoc, fromStream)");
    docOpt = readInto(aNewDoc, fromStream);
  }

  callback(docOpt);
}

std::vector<TopoDS_Shape> getShapesFromDoc(DocHandle const aDoc) {
  std::vector<TopoDS_Shape> shapes;

  TDF_Label mainLabel = aDoc->Main();
  Handle(XCAFDoc_ShapeTool) shapeTool =
      XCAFDoc_DocumentTool::ShapeTool(mainLabel);
  for (TDF_ChildIterator it(mainLabel, Standard_True); it.More(); it.Next()) {
    TDF_Label label = it.Value();
    if (!shapeTool->IsShape(label)) { continue; }
    TopoDS_Shape shape;
    if (!shapeTool->GetShape(label, shape) || shape.IsNull()) {
      std::cerr << "Failed to get shape from label." << std::endl;
      continue;
    }
    shapes.push_back(shape);
  }
  return shapes;
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
