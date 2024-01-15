#include "EmbeddedStepFile.hpp"
#include "StaircaseViewer.hpp"
#include <opencascade/Standard_Version.hxx>

void delayedLoadDemoStepFile(void *arg);
void delayedStartViewer(void *arg);

EMSCRIPTEN_KEEPALIVE int main() {
  std::string occt_ver_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);
  std::cout << occt_ver_str << std::endl;

  EM_ASM({ document.getElementById('version').innerHTML = UTF8ToString($0); },
         std::any_cast<std::string>(occt_ver_str).c_str());

  auto viewer = StaircaseViewer::Create("staircase-container");

  if (viewer == nullptr) {
    std::cerr << "Failed to create StaircaseViewer." << std::endl;
    return 1;
  }

  drawCheckerBoard(viewer->context->shaderProgram,
                   viewer->context->viewController->getWindowSize());

  emscripten_async_call(delayedStartViewer, viewer.get(), 500);
  emscripten_async_call(delayedLoadDemoStepFile, viewer.get(), 3000);

  return 0;
}

void delayedStartViewer(void *arg) {
  auto viewer = static_cast<StaircaseViewer *>(arg);
  viewer->initEmptyScene();
}
void delayedLoadDemoStepFile(void *arg) {
  auto viewer = static_cast<StaircaseViewer *>(arg);
  viewer->loadStepFile(embeddedStepFile);
  auto myData = std::make_shared<std::string>(embeddedStepFile);
  viewer->context->pushMessage({MessageType::SetStepFileContent, myData});
}
