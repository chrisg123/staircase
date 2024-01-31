#ifndef STAIRCASEVIEWER_HPP
#define STAIRCASEVIEWER_HPP
#include "ViewerContext.hpp"
#include "GraphicsUtilities.hpp"
#include <memory>
#include <optional>
#include <string>
#include <mutex>

class StaircaseViewer {
  static std::mutex startWorkerMutex;
  static std::atomic<bool> backgroundWorkerRunning;
  static pthread_t backgroundWorkerThread;

  static std::queue<Staircase::Message> backgroundQueue;
  static std::mutex backgroundQueueMutex;
  static std::condition_variable cv;

  static bool mainLoopSet;

public:

  StaircaseViewer(std::string const &containerId);

  static void ensureBackgroundWorker();
  static void pushBackground(const Staircase::Message& msg);
  static Staircase::Message popBackground();

  static void deleteViewer(StaircaseViewer* viewer);
  int createCanvas(std::string containerId, std::string canvasId);
  void displaySplashScreen();
  std::string getDemoStepFile();
  std::string getOCCTVersion();
  void initEmptyScene();
  ~StaircaseViewer();

  std::shared_ptr<ViewerContext> context;
  std::string getContainerId();

  int loadStepFile(std::string const &stepFileContent);
  static void handleMessages(void *arg);
  static void loadDefaultShaders(ViewerContext &context);
  static void cleanupDefaultShaders(ViewerContext &context);
  static void* backgroundWorker(void *arg);
  void setStepFileContent(const std::string& content);
  std::string getStepFileContent();
  void fitAllObjects ();
  void removeAllObjects();

private:
  std::string _stepFileContent;
  std::mutex stepFileContentMutex;

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
