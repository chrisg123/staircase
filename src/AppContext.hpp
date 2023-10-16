#ifndef APPCONTEXT_HPP
#define APPCONTEXT_HPP
#include "staircase.hpp"
#include <AIS_InteractiveContext.hxx>
#include <GLES2/gl2.h>
#include <V3d_View.hxx>
#include <any>
#include <mutex>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFApp_Application.hxx>
#include <queue>

class AppContext {
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
  Handle(TDocStd_Document) currentlyViewingDoc;

  bool showingSpinner = false;
  GLuint shaderProgram;
  GLint canvasWidth = 0;
  GLint canvasHeight = 0;
  std::unique_ptr<StaircaseViewController> viewController;
  Handle(V3d_Viewer) viewer;
  Handle(V3d_View) view;

  bool stepFileLoaded = false;
  bool shouldRotate = true;
  SpinnerParams spinnerParams;
  std::string canvasId;

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
  std::queue<Staircase::Message> messageQueue;
  std::mutex queueMutex;
};

#endif // APPCONTEXT_HPP
