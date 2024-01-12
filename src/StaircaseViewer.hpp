#ifndef STAIRCASEVIEWER_HPP
#define STAIRCASEVIEWER_HPP
#include "AppContext.hpp"
#include <memory>
#include <optional>
#include <string>

class StaircaseViewer {
public:
  static std::unique_ptr<StaircaseViewer, void (*)(StaircaseViewer *)>
  Create(std::string const &containerId);

  ~StaircaseViewer();

  std::shared_ptr<AppContext> context;

private:
  StaircaseViewer(std::string const &containerId);
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
