#include "StaircaseViewer.hpp"
#include "EmbeddedStepFile.hpp"
#include "GraphicsUtilities.hpp"
#include "OCCTUtilities.hpp"
#include <atomic>
#include <emscripten/threading.h>
#include <memory>
#include <opencascade/Standard_Version.hxx>
#include <optional>

EMSCRIPTEN_KEEPALIVE
StaircaseViewer::StaircaseViewer(std::string const &containerId) {

  emscripten_set_main_loop(dummyMainLoop, -1, 0);

  context = std::make_shared<AppContext>();
  context->canvasId = "staircase-canvas";

  context->viewController =
      std::make_unique<StaircaseViewController>(context->canvasId);

  createCanvas(containerId, context->canvasId);
  context->viewController->initWindow();
  setupWebGLContext(context->canvasId);
  context->viewController->initViewer();

  context->shaderProgram = createShaderProgram(
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

  context->pushMessage(MessageType::NextFrame); // kick off event loop
  emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VI, handleMessages,
                                              context.get());

  pthread_create(&backgroundWorkerThread, NULL,
                 StaircaseViewer::backgroundWorker, this);
}

EMSCRIPTEN_KEEPALIVE void StaircaseViewer::displaySplashScreen() {
  drawCheckerBoard(context->shaderProgram,
                   context->viewController->getWindowSize());
}

EMSCRIPTEN_KEEPALIVE std::string StaircaseViewer::getDemoStepFile() {
  return embeddedStepFile;
}

EMSCRIPTEN_KEEPALIVE std::string StaircaseViewer::getOCCTVersion() {
  return std::string(OCC_VERSION_COMPLETE);
}

EMSCRIPTEN_KEEPALIVE void StaircaseViewer::initEmptyScene() {
  context->pushMessage(
      *chain(MessageType::ClearScreen, MessageType::ClearScreen,
             MessageType::ClearScreen, MessageType::InitEmptyScene,
             MessageType::NextFrame));
}

EMSCRIPTEN_KEEPALIVE int
StaircaseViewer::loadStepFile(std::string const &stepFileContent) {
  if (!context->canLoadNewFile()) {
    std::cout << "Cannot load step file at this moment." << std::endl;
    return 1;
  }
  if (context->setCanLoadNewFile(false) != 0) {
    std::cerr << "setCanLoadNewFile(false) failed." << std::endl;
    return 1;
  }
  setStepFileContent(stepFileContent);
  context->pushBackground(MessageType::LoadStepFile);

  if (backgroundWorkerRunning) {
    backgroundWorkerRunning = true;
    pthread_create(&backgroundWorkerThread, NULL,
                   StaircaseViewer::backgroundWorker, this);
  }
  return 0;
}
void StaircaseViewer::setStepFileContent(std::string const &content) {
  std::lock_guard<std::mutex> lock(stepFileContentMutex);
  _stepFileContent = content;
}

std::string StaircaseViewer::getStepFileContent() {
  std::lock_guard<std::mutex> lock(stepFileContentMutex);
  return _stepFileContent;
}

StaircaseViewer::~StaircaseViewer() {}

void StaircaseViewer::createCanvas(std::string containerId,
                                   std::string canvasId) {
  EM_ASM(
      {
        if (typeof document == 'undefined') { return; }

        var divElement = document.getElementById(UTF8ToString($0));

        if (divElement) {
          var canvas = document.createElement('canvas');
          canvas.id = UTF8ToString($1);
          canvas.tabIndex = -1;
          canvas.oncontextmenu = function(event) { event.preventDefault(); };
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
        Module.canvas = canvas;
      },
      containerId.c_str(), canvasId.c_str());
}

void *StaircaseViewer::_loadStepFile(void *arg) {
  auto viewer = static_cast<StaircaseViewer *>(arg);
  auto context = viewer->context;


  context->showingSpinner = true;
  context->pushMessage({MessageType::DrawLoadingScreen});

  // Read STEP file and handle the result in the callback
  readStepFile(XCAFApp_Application::GetApplication(),
               viewer->getStepFileContent(),
               [&context](std::optional<Handle(TDocStd_Document)> docOpt) {
                 if (!docOpt.has_value()) {
                   std::cerr << "Failed to read STEP file: DocHandle is empty"
                             << std::endl;
                   return;
                 }
                 auto aDoc = docOpt.value();
                 std::cout << "STEP File Loaded!" << std::endl;
                 context->showingSpinner = false;
                 context->currentlyViewingDoc = aDoc;

                 context->pushMessage(
                     *chain(MessageType::ClearScreen, MessageType::ClearScreen,
                            MessageType::ClearScreen, MessageType::InitStepFile,
                            MessageType::NextFrame));
               });

  return nullptr;
}

void StaircaseViewer::fitAllObjects() {
  context->viewController->fitAllObjects(true);
}

void StaircaseViewer::removeAllObjects()
{
  context->viewController->removeAllObjects();
}

std::atomic<bool> isHandlingMessages{false};

void *StaircaseViewer::backgroundWorker(void *arg) {
  auto viewer = static_cast<StaircaseViewer *>(arg);

  while (true) {
    std::cout << "Background worker waiting for new messages..." << std::endl;
    Staircase::Message msg = viewer->context->popBackground();

    std::cout << "Background worker handling mesasge:" << std::endl;
    StaircaseViewer::_loadStepFile((void *)viewer);
  }
  return nullptr;
}

void StaircaseViewer::handleMessages(void *arg) {
  if (isHandlingMessages.exchange(true)) {
    std::cout << "Function is already being executed, skip this call"
              << std::endl;
    return;
  }

  auto context = static_cast<AppContext *>(arg);
  auto localQueue = context->drainMessageQueue();
  bool nextFrame = false;
  int const FPS60 = 1000 / 60;

  auto schedNextFrameWith = [&context, &nextFrame](MessageType::Type type) {
    Staircase::Message msg(type);
    context->pushMessage(msg);
    nextFrame = true;
  };

  while (!localQueue.empty()) {
    Staircase::Message message = localQueue.front();
    localQueue.pop();

    switch (message.type) {
    case MessageType::ClearScreen: clearCanvas(Colors::Platinum); break;
    case MessageType::InitEmptyScene:
      context->viewController->shouldRender = true;
      context->viewController->initScene();
      context->viewController->updateView();
      break;
    case MessageType::InitStepFile:
      context->viewController->initStepFile(context->currentlyViewingDoc);
      break;
    case MessageType::NextFrame: {

      if (context->isMessageQueueEmpty()) {
        schedNextFrameWith(MessageType::NextFrame);
      } // else { /* Next frame inevitable. */ }
      break;
    }
    case MessageType::DrawLoadingScreen: {
      clearCanvas(Colors::Platinum);
      if (context->viewController->shouldRender) {
        context->shaderProgram = createShaderProgram(
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
      }
      context->viewController->shouldRender = false;

      drawLoadingScreen(context->shaderProgram, context->spinnerParams);

      if (context->showingSpinner) {
        schedNextFrameWith(MessageType::DrawLoadingScreen);
      } else {
        context->viewController->shouldRender = true;
        nextFrame = false;
      }
      break;
    }

    default: std::cout << "Unhandled MessageType::" << std::endl; break;
    }

    if (message.nextMessage) {
      context->pushMessage(*message.nextMessage);
      nextFrame = true;
    }
  }

  isHandlingMessages = false;
  if (nextFrame) { emscripten_set_timeout(handleMessages, FPS60, context); }
}

extern "C" void dummyMainLoop() { emscripten_cancel_main_loop(); }
void dummyDeleter(StaircaseViewer *) {}

EMSCRIPTEN_BINDINGS(staircase) {
  emscripten::class_<StaircaseViewer>("StaircaseViewer")
      .constructor<std::string const &>()
      .function("displaySplashScreen", &StaircaseViewer::displaySplashScreen)
      .function("initEmptyScene", &StaircaseViewer::initEmptyScene)
      .function("getDemoStepFile", &StaircaseViewer::getDemoStepFile)
      .function("getOCCTVersion", &StaircaseViewer::getOCCTVersion)
      .function("fitAllObjects", &StaircaseViewer::fitAllObjects)
      .function("removeAllObjects", &StaircaseViewer::removeAllObjects)
      .function("loadStepFile", &StaircaseViewer::loadStepFile);
}
