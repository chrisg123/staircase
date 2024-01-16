#ifndef STAIRCASEVIEWER_HPP
#define STAIRCASEVIEWER_HPP
#include "AppContext.hpp"
#include "GraphicsUtilities.hpp"
#include <memory>
#include <optional>
#include <string>

class StaircaseViewer {
public:
  void createCanvas(std::string containerId, std::string canvasId);
  static std::unique_ptr<StaircaseViewer, void (*)(StaircaseViewer *)>
  Create(std::string const &containerId);
  void initEmptyScene();
  ~StaircaseViewer();

  std::shared_ptr<AppContext> context;
  std::string stepFileContent;

  void loadStepFile(std::string const &stepFileContent);
  static void handleMessages(void *arg);

private:
  StaircaseViewer(std::string const &containerId);
  static void* _loadStepFile(void *args);
};

extern "C" void dummyMainLoop();
void dummyDeleter(StaircaseViewer *);

inline bool arePthreadsEnabled() {
#ifdef __EMSCRIPTEN_PTHREADS__
  return true;
#else
  return false;
#endif
}

#endif // STAIRCASEVIEWER_HPP
