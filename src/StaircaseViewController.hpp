#ifndef STAIRCASEVIEWCONTROLLER_HPP
#define STAIRCASEVIEWCONTROLLER_HPP
#include <AIS_ViewController.hxx>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/html5.h>
#include <opencascade/AIS_ViewCube.hxx>
#include <opencascade/Prs3d_TextAspect.hxx>
#include <opencascade/TDocStd_Document.hxx>

class StaircaseViewController : protected AIS_ViewController {
public:
  StaircaseViewController(std::string const &canvasId)
      : canvasId(canvasId), devicePixelRatio(1), updateRequestCount(0) {}
  virtual ~StaircaseViewController() {}
  void initWindow();
  bool initViewer();
  void initPixelScaleRatio();
  void initScene();
  void redrawView();
  void updateView();
  void initStepFile(Handle(TDocStd_Document) aDoc);
  char const *getCanvasTag();
  EM_BOOL onMouseEvent(int eventType, EmscriptenMouseEvent const *event);
  EM_BOOL onWheelEvent(int theEventType, EmscriptenWheelEvent const *theEvent);
  EM_BOOL onResizeEvent(int theEventType, EmscriptenUiEvent const *theEvent);
  static void onRedrawView(void *view);
  virtual void ProcessInput() override;

  Handle(V3d_View) getView() const;
  void setView(Handle(V3d_View) const &view);

  Handle(AIS_InteractiveContext) getAISContext() const;
  void setAISContext(Handle(AIS_InteractiveContext) const &aisContext);

  bool shouldRender;

  const Graphic3d_Vec2i& getWindowSize() const;
private:
  std::string canvasId;
  std::string prefixedCanvasId;

  float devicePixelRatio;
  unsigned int updateRequestCount;
  Graphic3d_Vec2i windowSize;

  Handle(AIS_InteractiveContext) aisContext;
  Handle(Prs3d_TextAspect) textAspect;
  Handle(AIS_ViewCube) viewCube;
  Handle(V3d_View) view;

};
#endif // STAIRCASEVIEWCONTROLLER_HPP
