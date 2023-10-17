#include "AppContext.hpp"
#include "EmbeddedStepFile.hpp"
#include "GraphicsUtilities.hpp"
#include "OCCTUtilities.hpp"
#include "staircase.hpp"
#include <any>
#include <chrono>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/threading.h>
#include <iostream>
#include <opencascade/Standard_Version.hxx>
#include <optional>
#include <queue>
#include <thread>
#include <memory>

bool arePthreadsEnabled() {
#ifdef __EMSCRIPTEN_PTHREADS__
  return true;
#else
  return false;
#endif
}
using DocHandle = Handle(TDocStd_Document);
void main_loop(void *arg);

void handleMessages(void *arg);

void draw(AppContext &context);

void bootstrap(void *arg);
void *loadStepFile(void *arg);

template <typename... MessageTypes>
std::shared_ptr<Staircase::Message> chain(MessageTypes... types) {
  std::shared_ptr<Staircase::Message> head = nullptr;
  std::shared_ptr<Staircase::Message> current = nullptr;

  auto createAndLink = [&head, &current](MessageType::Type type) {
    auto newMsg = std::make_shared<Staircase::Message>(type);
    if (!head) {
      head = newMsg;
    } else {
      current->nextMessage = newMsg;
    }
    current = newMsg;
  };
  (createAndLink(types), ...);
  return head;
}

extern "C" void dummyMainLoop() { emscripten_cancel_main_loop(); }

EMSCRIPTEN_KEEPALIVE int main() {
  AppContext *context = new AppContext;
  std::string occt_ver_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);
  std::cout << occt_ver_str << std::endl;

  EM_ASM({ document.getElementById('version').innerHTML = UTF8ToString($0); },
         std::any_cast<std::string>(occt_ver_str).c_str());

  if (!arePthreadsEnabled()) {
    std::cerr << "Pthreads are required." << std::endl;
    return 1;
  }
  std::cout << "Pthreads enabled" << std::endl;

  std::string containerId = "staircase-container";
  context->canvasId = "staircase-canvas";

  context->viewController =
      std::make_unique<StaircaseViewController>(context->canvasId);

  emscripten_set_main_loop(dummyMainLoop, -1, 0);

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

  drawCheckerBoard(context->shaderProgram,
                   context->viewController->getWindowSize());

  int const INITIAL_DELAY_MS = 1000;
  emscripten_async_call(bootstrap, context, INITIAL_DELAY_MS);

  return 0;
}

void bootstrap(void *arg) {
  auto context = static_cast<AppContext *>(arg);
  pthread_t worker;
  pthread_create(&worker, NULL, loadStepFile, context);
}

void *loadStepFile(void *arg) {
  auto context = static_cast<AppContext *>(arg);
  std::cout << "loadStepFile" << std::endl;
  std::string stepFileStr = embeddedStepFile;

  context->showingSpinner = true;
  context->pushMessage({MessageType::DrawLoadingScreen});

  auto myData = std::make_shared<std::string>(stepFileStr);
  context->pushMessage({MessageType::SetStepFileContent, myData});

  emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VI, handleMessages,
                                              (void *)context);

  // Read STEP file and handle the result in the callback
  readStepFile(XCAFApp_Application::GetApplication(), stepFileStr,
               [&context](std::optional<DocHandle> docOpt) {
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

                 context->pushMessage(*chain(
                     MessageType::ClearScreen, MessageType::ClearScreen,
                     MessageType::ClearScreen, MessageType::InitDemoScene,
                     MessageType::RedrawView));

                 emscripten_async_run_in_main_runtime_thread(
                     EM_FUNC_SIG_VI, handleMessages, (void *)context);
               });

  return NULL;
}

EMSCRIPTEN_KEEPALIVE void handleMessages(void *arg) {
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
    case MessageType::InitDemoScene:
      context->viewController->initDemoScene();
      context->viewController->initStepFile(context->currentlyViewingDoc);
      break;
    case MessageType::RedrawView: {
      context->viewController->redrawView();
      schedNextFrameWith(MessageType::RedrawView);
      break;
    }
    case MessageType::DrawLoadingScreen: {
      clearCanvas(Colors::Platinum);
      drawLoadingScreen(context->shaderProgram, context->spinnerParams);

      if (context->showingSpinner) {
        schedNextFrameWith(MessageType::DrawLoadingScreen);
      } else {
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
