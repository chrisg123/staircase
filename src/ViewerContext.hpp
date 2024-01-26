#ifndef VIEWERCONTEXT_HPP
#define VIEWERCONTEXT_HPP
#include "staircase.hpp"
#include <AIS_InteractiveContext.hxx>
#include <GLES2/gl2.h>
#include <V3d_View.hxx>
#include <any>
#include <mutex>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFApp_Application.hxx>
#include <queue>

class ViewerContext {
public:


  void pushMessage(Staircase::Message const &msg) {
    std::lock_guard<std::mutex> lock(queueMutex);
    messageQueue.push(msg);
  }

  std::queue<Staircase::Message> drainMessageQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::queue<Staircase::Message> localQueue;
    std::swap(localQueue, messageQueue);
    return localQueue;
  }

  bool isMessageQueueEmpty() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return messageQueue.empty();
  }

  void pushBackground(const Staircase::Message& msg) {
    std::unique_lock<std::mutex> lock(backgroundQueueMutex);
    backgroundQueue.push(msg);
    cv.notify_one();
  }

  Staircase::Message popBackground() {
    std::unique_lock<std::mutex> lock(backgroundQueueMutex);
    cv.wait(lock, [this]{ return !backgroundQueue.empty(); });
    Staircase::Message msg = backgroundQueue.front();
    backgroundQueue.pop();
    return msg;
  }

  Handle(TDocStd_Document) currentlyViewingDoc;

  bool showingSpinner = false;
  GLuint shaderProgram;
  GLint canvasWidth = 0;
  GLint canvasHeight = 0;
  std::unique_ptr<StaircaseViewController> viewController;

  bool stepFileLoaded = false;
  bool shouldRotate = true;
  SpinnerParams spinnerParams;
  std::string containerId;
  std::string canvasId;

  Handle(V3d_View) getView() const {
    if (viewController) { return viewController->getView(); }
    return Handle(V3d_View)();
  }

  void setView(Handle(V3d_View) const &view) {
    if (viewController) {
      viewController->setView(view);
    }
  }

  int setCanLoadNewFile(bool value) {
    if (viewController) {
      viewController->setCanLoadNewFile(value);
      return 0; // success
    }
    return 1; // failure
  }

  bool canLoadNewFile() {
    if (viewController) { return viewController->canLoadNewFile(); }
    return false;
  }

  Handle(AIS_InteractiveContext) getAISContext() const {
    if (viewController) { return viewController->getAISContext(); }
    return Handle(AIS_InteractiveContext)();
  }
  void setAISContext(const Handle(AIS_InteractiveContext) &aisContext) {
    if (viewController) {
      viewController->setAISContext(aisContext);
    }
  }

private:
  Handle(V3d_View) view;
  std::queue<Staircase::Message> messageQueue;
  std::mutex queueMutex;

  std::queue<Staircase::Message> backgroundQueue;
  std::mutex backgroundQueueMutex;
  std::condition_variable cv;
};

#endif // VIEWERCONTEXT_HPP
