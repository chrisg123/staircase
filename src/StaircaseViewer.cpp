#include "StaircaseViewer.hpp"
#include "GraphicsUtilities.hpp"
#include "OCCTUtilities.hpp"
#include <emscripten/threading.h>
#include <memory>
#include <optional>

std::unique_ptr<StaircaseViewer, void (*)(StaircaseViewer *)>
StaircaseViewer::Create(std::string const &containerId) {
  if (!arePthreadsEnabled()) {
    std::cerr << "Pthreads are required." << std::endl;
    return std::unique_ptr<StaircaseViewer, void (*)(StaircaseViewer *)>(
        nullptr, dummyDeleter);
  }

  // Using a custom dummy deleter to manually manage StaircaseViewer's lifetime
  // while utilizing the managed pointer interface of std::unique_ptr.
  return std::unique_ptr<StaircaseViewer, void (*)(StaircaseViewer *)>(
      new StaircaseViewer(containerId), dummyDeleter);
}

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

  EM_ASM(Module['noExitRuntime'] = true);

  context->pushMessage(MessageType::NextFrame); // kick off event loop

  emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VI, handleMessages,
                                              context.get());
}
void StaircaseViewer::initEmptyScene() {
  context->pushMessage(
      *chain(MessageType::ClearScreen, MessageType::ClearScreen,
             MessageType::ClearScreen, MessageType::InitEmptyScene,
             MessageType::NextFrame));
}
StaircaseViewer::~StaircaseViewer() {}

void StaircaseViewer::loadStepFile(std::string const &stepFileContent) {
  this->stepFileContent = stepFileContent;
  pthread_t worker;
  pthread_create(&worker, NULL, StaircaseViewer::_loadStepFile, this);
}

void *StaircaseViewer::_loadStepFile(void *arg) {
  auto viewer = static_cast<StaircaseViewer *>(arg);
  auto context = viewer->context;

  context->showingSpinner = true;
  context->pushMessage({MessageType::DrawLoadingScreen});

  // Read STEP file and handle the result in the callback
  readStepFile(XCAFApp_Application::GetApplication(), viewer->stepFileContent,
               [&context](std::optional<Handle(TDocStd_Document)> docOpt) {
                 if (!docOpt.has_value()) {
                   std::cerr << "Failed to read STEP file: DocHandle is empty"
                             << std::endl;
                   return;
                 }
                 auto aDoc = docOpt.value();
                 printLabels(aDoc->Main());
                 std::cout << "STEP File Loaded!" << std::endl;
                 context->showingSpinner = false;
                 context->currentlyViewingDoc = aDoc;

                 context->pushMessage(
                     *chain(MessageType::ClearScreen, MessageType::ClearScreen,
                            MessageType::ClearScreen, MessageType::InitStepFile,
                            MessageType::NextFrame));

                 emscripten_async_run_in_main_runtime_thread(
                     EM_FUNC_SIG_VI, handleMessages, context.get());
               });

  return nullptr;
}

void StaircaseViewer::handleMessages(void *arg) {
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
      schedNextFrameWith(MessageType::NextFrame);
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
    case MessageType::SetStepFileContent: {
      auto stepFileStr = std::static_pointer_cast<std::string>(message.data);
      EM_ASM(
          { document.getElementById('stepText').innerHTML = UTF8ToString($0); },
          stepFileStr->c_str());

      break;
    }
    default: std::cout << "Unhandled MessageType::" << std::endl; break;
    }

    if (message.nextMessage) {
      context->pushMessage(*message.nextMessage);
      nextFrame = true;
    }
  }

  if (nextFrame) { emscripten_set_timeout(handleMessages, FPS60, context); }
}

extern "C" void dummyMainLoop() { emscripten_cancel_main_loop(); }
void dummyDeleter(StaircaseViewer *) {}
