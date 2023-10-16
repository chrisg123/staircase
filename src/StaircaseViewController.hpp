#include <AIS_ViewController.hxx>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/html5.h>
#include <opencascade/AIS_ViewCube.hxx>
#include <opencascade/Prs3d_TextAspect.hxx>
class StaircaseViewController : protected AIS_ViewController {
public:
  StaircaseViewController(std::string const &canvasId) : canvasId(canvasId) {}
  virtual ~StaircaseViewController() {}
  void run();
  void initWindow();
  bool initViewer();
  void initPixelScaleRatio();
  void initDemoScene();
  void redrawView();
  void updateView();

  char const *getCanvasTag();
  EM_BOOL onMouseEvent(int eventType, EmscriptenMouseEvent const *event);
  EM_BOOL onWheelEvent(int theEventType, EmscriptenWheelEvent const *theEvent);
  EM_BOOL onResizeEvent(int theEventType, EmscriptenUiEvent const *theEvent);

private:
  std::string canvasId;
  std::string prefixedCanvasId;

  float devicePixelRatio;
  Graphic3d_Vec2i windowSize;

  Handle(AIS_InteractiveContext) aisContext;
  Handle(Prs3d_TextAspect) textAspect;
  Handle(AIS_ViewCube) viewCube;
  Handle(V3d_View) view;
};
