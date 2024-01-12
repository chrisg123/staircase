#include "StaircaseViewer.hpp"
#include "GraphicsUtilities.hpp"
#include <memory>

std::unique_ptr<StaircaseViewer, void (*)(StaircaseViewer *)>
StaircaseViewer::Create(std::string const &containerId) {
  if (!arePthreadsEnabled()) {
    std::cerr << "Pthreads are required." << std::endl;
    return std::unique_ptr<StaircaseViewer, void (*)(StaircaseViewer *)>(
        nullptr, dummyDeleter);
  }

  // Using a custom dummy deleter to manage the lifetime of StaircaseViewer
  // manually.
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
}

StaircaseViewer::~StaircaseViewer() {}

extern "C" void dummyMainLoop() { emscripten_cancel_main_loop(); }
void dummyDeleter(StaircaseViewer *) {
  std::cout << "*** dummyDeleter called" << std::endl;
}
