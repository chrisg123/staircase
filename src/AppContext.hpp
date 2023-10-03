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
  AppContext() { app = XCAFApp_Application::GetApplication(); }

  Handle(XCAFApp_Application) getApp() const { return app; }

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
  RenderingMode renderingMode = RenderingMode::None;
  Handle(TDocStd_Document) currentlyViewingDoc;

  GLuint shaderProgram;
  GLint canvasWidth = 0;
  GLint canvasHeight = 0;

  Handle(V3d_Viewer) viewer;
  Handle(AIS_InteractiveContext) viewerContext;
  Handle(V3d_View) view;
  Handle(AIS_InteractiveContext) aisContext;

  bool stepFileLoaded = false;
  bool occtComponentsInitialized = false;
  bool shouldRotate = true;
  SpinnerParams spinnerParams;
  std::string canvasId;

private:
  Handle(XCAFApp_Application) app;
  std::queue<Staircase::Message> messageQueue;
  std::mutex queueMutex;
};

#endif //APPCONTEXT_HPP
