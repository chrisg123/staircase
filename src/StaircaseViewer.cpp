#include "StaircaseViewer.hpp"
#include "GraphicsUtilities.hpp"
#include "OCCTUtilities.hpp"
#include <atomic>
#include <emscripten/threading.h>
#include <memory>
#include <opencascade/Standard_Version.hxx>
#include <optional>

#ifndef DIST_BUILD
#include "EmbeddedStepFile.hpp"
#endif

std::mutex StaircaseViewer::startWorkerMutex;
std::atomic<bool> StaircaseViewer::backgroundWorkerRunning = false;
pthread_t StaircaseViewer::backgroundWorkerThread;
std::queue<Staircase::Message> StaircaseViewer::backgroundQueue;
std::mutex StaircaseViewer::backgroundQueueMutex;
std::condition_variable StaircaseViewer::cv;
bool StaircaseViewer::mainLoopSet = false;

// clang-format off
EM_JS(const char*, generate_uuid_js, (), {
  var uuid = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
    var r = Math.random() * 16 | 0;
    var v = c == 'x' ? r : (r & 0x3 | 0x8);
    return v.toString(16);
  });

  var length = lengthBytesUTF8(uuid) + 1;
  var stringOnWasmHeap = _malloc(length);
  stringToUTF8(uuid, stringOnWasmHeap, length);
  return stringOnWasmHeap;
});
// clang-format on

EMSCRIPTEN_KEEPALIVE std::string generate_uuid() {
  char const *uuid_c_str = generate_uuid_js();
  std::string uuid(uuid_c_str);
  std::free(const_cast<char *>(uuid_c_str));
  return uuid;
}

EMSCRIPTEN_KEEPALIVE
StaircaseViewer::StaircaseViewer(std::string const &containerId) {

  if (!mainLoopSet) {
    mainLoopSet = true;
    emscripten_set_main_loop(dummyMainLoop, -1, 0);
  }

  context = std::make_shared<ViewerContext>();
  context->canvasId = "staircase-canvas-" + generate_uuid();

  context->viewController =
      std::make_unique<StaircaseViewController>(context->canvasId);

  if (createCanvas(containerId, context->canvasId) != 0) {
    std::cerr << "Failed to create canvas. Initialization aborted." << std::endl;
    return;
  }

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

  StaircaseViewer::ensureBackgroundWorker();
}

EMSCRIPTEN_KEEPALIVE void StaircaseViewer::displaySplashScreen() {
  drawCheckerBoard(context->shaderProgram,
                   context->viewController->getWindowSize());
}

EMSCRIPTEN_KEEPALIVE std::string StaircaseViewer::getDemoStepFile() {
#ifndef DIST_BUILD
  return embeddedStepFile;
#else
  std::cout << "Demo file not available in the distribution build." << std::endl;
  return "";
#endif
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

EMSCRIPTEN_KEEPALIVE std::string StaircaseViewer::getContainerId() {
  return this->context->containerId;
}

EMSCRIPTEN_KEEPALIVE int
StaircaseViewer::loadStepFile(std::string const &stepFileContent) {
  if (stepFileContent.empty()) {
    std::cerr << "Step file content is empty." << std::endl;
    return 1;
  }
  if (!context->canLoadNewFile()) {
    std::cout << "Cannot load step file at this moment." << std::endl;
    return 1;
  }
  if (context->setCanLoadNewFile(false) != 0) {
    std::cerr << "setCanLoadNewFile(false) failed." << std::endl;
    return 1;
  }
  setStepFileContent(stepFileContent);

  Staircase::Message message(MessageType::LoadStepFile, this);
  StaircaseViewer::pushBackground(message);
  StaircaseViewer::ensureBackgroundWorker();

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

int StaircaseViewer::createCanvas(std::string containerId,
                                   std::string canvasId) {
  return EM_ASM_INT(
      {
        if (typeof document == 'undefined') { return; }

        var divElement = document.getElementById(UTF8ToString($0));

        if (!divElement) {
          console.error("Div element with id '" + UTF8ToString($0) + "' not found.");
          Module.canvas = null;
          return 1;
        }
        var canvas = document.createElement('canvas');
        canvas.id = UTF8ToString($1);
        canvas.className = "staircase-canvas";
        canvas.tabIndex = -1;
        canvas.oncontextmenu = function(event) { event.preventDefault(); };

        // Align canvas size with device pixels
        var computedStyle = window.getComputedStyle(divElement);
        var cssWidth = parseInt(computedStyle.getPropertyValue('width'), 10);
        var cssHeight =
            parseInt(computedStyle.getPropertyValue('height'), 10);
        var devicePixelRatio = window.devicePixelRatio || 1;

        canvas.width = cssWidth * devicePixelRatio;
        canvas.height = cssHeight * devicePixelRatio;

        canvas.style.width = "100%";
        canvas.style.height = "100%";

        divElement.appendChild(canvas);

        Module.canvas = canvas;
        return 0;
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
                   context->showingSpinner = false;
                   context->pushMessage(*chain(
                       MessageType::ClearScreen, MessageType::ClearScreen,
                       MessageType::ClearScreen, MessageType::InitEmptyScene,
                       MessageType::NextFrame));
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

void StaircaseViewer::removeAllObjects() {
  context->viewController->removeAllObjects();
}

std::atomic<bool> isHandlingMessages{false};

void *StaircaseViewer::backgroundWorker(void *) {
  while (true) {
    Staircase::Message msg = StaircaseViewer::popBackground();
    StaircaseViewer::_loadStepFile(msg.data);
  }
  return nullptr;
}

void StaircaseViewer::ensureBackgroundWorker() {
  std::lock_guard<std::mutex> guard(StaircaseViewer::startWorkerMutex);
  if (!StaircaseViewer::backgroundWorkerRunning) {
    backgroundWorkerRunning = true;
    pthread_create(&backgroundWorkerThread, NULL,
                   StaircaseViewer::backgroundWorker, NULL);
  }
}

void StaircaseViewer::pushBackground(const Staircase::Message& msg) {
  std::unique_lock<std::mutex> lock(backgroundQueueMutex);
  backgroundQueue.push(msg);
  cv.notify_one();
}

Staircase::Message StaircaseViewer::popBackground() {
  std::unique_lock<std::mutex> lock(backgroundQueueMutex);
  cv.wait(lock, []{ return !backgroundQueue.empty(); });
  Staircase::Message msg = backgroundQueue.front();
  backgroundQueue.pop();
  return msg;
}

void StaircaseViewer::handleMessages(void *arg) {
  if (isHandlingMessages.exchange(true)) {
    return;
  }

  auto context = static_cast<ViewerContext *>(arg);
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
      .function("loadStepFile", &StaircaseViewer::loadStepFile)
      .function("getContainerId", &StaircaseViewer::getContainerId);
}
