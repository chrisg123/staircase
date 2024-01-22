#include "StaircaseViewController.hpp"
#include "AppContext.hpp"
#include "OCCTUtilities.hpp"
#include "staircase.hpp"
#include <AIS_ViewCube.hxx>
#include <Wasm_Window.hxx>
#include <opencascade/AIS_InteractiveContext.hxx>
#include <opencascade/AIS_Shape.hxx>
#include <opencascade/OpenGl_GraphicDriver.hxx>
#include <opencascade/Prs3d_DatumAspect.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/TopoDS_Shape.hxx>
#include <opencascade/V3d_View.hxx>

// Update canvas bounding rectangle.
EM_JS(void, jsUpdateBoundingClientRect, (),
      { Module._myCanvasRect = Module.canvas.getBoundingClientRect(); });

// Get canvas bounding top.
EM_JS(int, jsGetBoundingClientTop, (),
      { return Math.round(Module._myCanvasRect.top); });

// Get canvas bounding left.
EM_JS(int, jsGetBoundingClientLeft, (),
      { return Math.round(Module._myCanvasRect.left); });

void StaircaseViewController::initWindow() {
  devicePixelRatio = emscripten_get_device_pixel_ratio();

  auto canvasTarget = getCanvasTag();
  auto windowTarget = EMSCRIPTEN_EVENT_TARGET_WINDOW;
  const EM_BOOL useCapture = EM_TRUE;

  auto mouseCallback = [](int eventType, EmscriptenMouseEvent const *event,
                          void *userData) -> EM_BOOL {
    return static_cast<StaircaseViewController *>(userData)->onMouseEvent(
        eventType, event);
  };

  auto wheelCallback = [](int eventType, EmscriptenWheelEvent const *event,
                          void *userData) -> EM_BOOL {
    return static_cast<StaircaseViewController *>(userData)->onWheelEvent(
        eventType, event);
  };

  auto touchCallback = [](int eventType, EmscriptenTouchEvent const *event,
                          void *userData) -> EM_BOOL {
    return static_cast<StaircaseViewController *>(userData)->onTouchEvent(
        eventType, event);
  };

  auto focusCallback = [](int eventType, EmscriptenFocusEvent const *event,
                          void *userData) -> EM_BOOL {
    return static_cast<StaircaseViewController *>(userData)->onFocusEvent(
        eventType, event);
  };

  auto keyDownCallback = [](int eventType, EmscriptenKeyboardEvent const *event,
                          void *userData) -> EM_BOOL {
    return static_cast<StaircaseViewController *>(userData)->onKeyDownEvent(
        eventType, event);
  };

  auto keyUpCallback = [](int eventType, EmscriptenKeyboardEvent const *event,
                          void *userData) -> EM_BOOL {
    return static_cast<StaircaseViewController *>(userData)->onKeyUpEvent(
        eventType, event);
  };

  auto resizeCallback = [](int eventType, EmscriptenUiEvent const *event,
                           void *userData) -> EM_BOOL {
    return static_cast<StaircaseViewController *>(userData)->onResizeEvent(
        eventType, event);
  };

  // clang-format off
  emscripten_set_resize_callback     (windowTarget, this, useCapture, resizeCallback);
  emscripten_set_mousedown_callback  (canvasTarget, this, useCapture, mouseCallback);
  emscripten_set_mouseup_callback    (windowTarget, this, useCapture, mouseCallback);
  emscripten_set_mousemove_callback  (windowTarget, this, useCapture, mouseCallback);
  emscripten_set_dblclick_callback   (canvasTarget, this, useCapture, mouseCallback);
  emscripten_set_click_callback      (canvasTarget, this, useCapture, mouseCallback);
  emscripten_set_mouseenter_callback (canvasTarget, this, useCapture, mouseCallback);
  emscripten_set_wheel_callback      (canvasTarget, this, useCapture, wheelCallback);

  emscripten_set_touchstart_callback (canvasTarget, this, useCapture, touchCallback);
  emscripten_set_touchend_callback   (canvasTarget, this, useCapture, touchCallback);
  emscripten_set_touchmove_callback  (canvasTarget, this, useCapture, touchCallback);
  emscripten_set_touchcancel_callback(canvasTarget, this, useCapture, touchCallback);

  emscripten_set_keydown_callback    (canvasTarget, this, useCapture, keyDownCallback);
  emscripten_set_keyup_callback      (canvasTarget, this, useCapture, keyUpCallback);

  emscripten_set_focusout_callback   (canvasTarget, this, useCapture, focusCallback);

  // clang-format on
}

bool StaircaseViewController::initViewer() {
  std::cout << "StaircaseViewController::initViewer()" << std::endl;
  Handle(Aspect_DisplayConnection) aDisp;
  Handle(OpenGl_GraphicDriver) aDriver = new OpenGl_GraphicDriver(aDisp, false);
  aDriver->ChangeOptions().buffersNoSwap = true;
  aDriver->ChangeOptions().buffersOpaqueAlpha = true;

  if (!aDriver->InitContext()) {
    std::cerr << "Error: EGL initialization failed" << std::endl;
    return false;
  }

  Handle(V3d_Viewer) aViewer = new V3d_Viewer(aDriver);
  aViewer->SetComputedMode(false);
  aViewer->SetDefaultShadingModel(Graphic3d_TypeOfShadingModel_Phong);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  for (V3d_ListOfLight::Iterator aLightIter(aViewer->ActiveLights());
       aLightIter.More(); aLightIter.Next()) {
    const Handle(V3d_Light) &aLight = aLightIter.Value();
    if (aLight->Type() == Graphic3d_TypeOfLightSource_Directional) {
      aLight->SetCastShadows(true);
    }
  }
  auto canvasTarget = getCanvasTag();

  Handle(Wasm_Window) aWindow = new Wasm_Window(canvasTarget);
  aWindow->Size(windowSize.x(), windowSize.y());

  textAspect = new Prs3d_TextAspect();
  textAspect->SetFont(Font_NOF_ASCII_MONO);
  textAspect->SetHeight(12);
  textAspect->Aspect()->SetColor(Quantity_NOC_GRAY95);
  textAspect->Aspect()->SetColorSubTitle(Quantity_NOC_BLACK);
  textAspect->Aspect()->SetDisplayType(Aspect_TODT_SHADOW);
  textAspect->Aspect()->SetTextFontAspect(Font_FA_Bold);
  textAspect->Aspect()->SetTextZoomable(false);
  textAspect->SetHorizontalJustification(Graphic3d_HTA_LEFT);
  textAspect->SetVerticalJustification(Graphic3d_VTA_BOTTOM);

  view = aViewer->CreateView();
  view->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
  view->SetImmediateUpdate(false);
  view->ChangeRenderingParams().IsShadowEnabled = false;
  view->ChangeRenderingParams().Resolution =
      (unsigned int)96.0 * devicePixelRatio + 0.5;
  view->ChangeRenderingParams().ToShowStats = false;
  view->ChangeRenderingParams().StatsTextAspect = textAspect->Aspect();
  view->ChangeRenderingParams().StatsTextHeight = textAspect->Height();
  view->SetWindow(aWindow);

  aisContext = new AIS_InteractiveContext(aViewer);
  initPixelScaleRatio();

  return true;
}

void StaircaseViewController::initPixelScaleRatio() {
  if (!view.IsNull()) {
    view->ChangeRenderingParams().Resolution =
        (unsigned int)(96.0 * devicePixelRatio + 0.5);
  }
  if (!aisContext.IsNull()) {
    aisContext->SetPixelTolerance((int(devicePixelRatio) * 6.0));
    if (!viewCube.IsNull()) {
      static double const THE_CUBE_SIZE = 60.0;
      viewCube->SetSize(devicePixelRatio * THE_CUBE_SIZE, false);
      viewCube->SetBoxFacetExtension(viewCube->Size() * 0.15);
      viewCube->SetAxesPadding(viewCube->Size());
      viewCube->SetFontHeight(THE_CUBE_SIZE * 0.16);
      if (viewCube->HasInteractiveContext()) {
        aisContext->Redisplay(viewCube, false);
      }
    }
  }
}

void StaircaseViewController::initScene() {
  viewCube = new AIS_ViewCube();
  initPixelScaleRatio();
  viewCube->SetTransformPersistence(new Graphic3d_TransformPers(
      Graphic3d_TMF_TriedronPers, Aspect_TOTP_RIGHT_LOWER,
      Graphic3d_Vec2i(100, 100)));

  viewCube->Attributes()->SetDatumAspect(new Prs3d_DatumAspect());
  viewCube->Attributes()->DatumAspect()->SetTextAspect(textAspect);
  viewCube->SetViewAnimation(myViewAnimation);
  viewCube->SetFixedAnimationLoop(false);
  viewCube->SetAutoStartAnimation(true);
  aisContext->Display(viewCube, false);
}

void StaircaseViewController::initStepFile(Handle(TDocStd_Document) aDoc) {
  if (aDoc.IsNull() || aisContext.IsNull()) {
    std::cerr << "No document or AIS context." << std::endl;
  }

  for (auto const &shape : activeShapes) {
    aisContext->Remove(shape, false);
    aisContext->Erase(shape, false);
  }
  if (!activeShapes.empty()) {
    activeShapes.clear();
    this->updateView();
  }

  std::vector<TopoDS_Shape> shapes = getShapesFromDoc(aDoc);

  for (auto const &shape : shapes) {
    Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
    aisContext->SetDisplayMode(aisShape, AIS_SHADED_MODE, Standard_True);
    aisContext->Display(aisShape, Standard_True);

    std::optional<Quantity_Color> optColor = getShapeColor(aDoc, shape);

    if (optColor.has_value()) {
      Quantity_Color aColor = optColor.value();
      aisContext->SetColor(aisShape, aColor, Standard_True);
    }
    activeShapes.push_back(aisShape);
  }

  this->FitAllAuto(aisContext, view);
  this->updateView();
}

void StaircaseViewController::setCanLoadNewFile(bool value) {
  std::lock_guard<std::mutex> lock(fileLoadMutex);
  _canLoadNewFile = value;
}

bool StaircaseViewController::canLoadNewFile() {
  std::lock_guard<std::mutex> lock(fileLoadMutex);
  return _canLoadNewFile;
}

void StaircaseViewController::ProcessInput() {
  if (shouldRender && !view.IsNull()) {
    // Schedule canvas redraw post user input, aligned with animation frame.
    if (++updateRequestCount == 1) {
      emscripten_async_call(onRedrawView, this, -1);
    }
  }
}

void StaircaseViewController::onRedrawView(void *view) {
  return ((StaircaseViewController *)view)->redrawView();
}

void StaircaseViewController::updateView() {
  if (!view.IsNull()) {
    view->Invalidate();
    ProcessInput();
  }
}

void StaircaseViewController::redrawView() {
  if (!view.IsNull()) {
    updateRequestCount = 0;
    FlushViewEvents(aisContext, view, true);
  }
  setCanLoadNewFile(true);
}

void StaircaseViewController::fitAllObjects(bool withAuto) {
  if (withAuto) {
    this->FitAllAuto(aisContext, view);
  } else {
    view->FitAll(0.01, false);
  }
  this->updateView();
}

EM_BOOL
StaircaseViewController::onMouseEvent(int eventType,
                                      EmscriptenMouseEvent const *event) {
  if (view.IsNull()) { return EM_FALSE; }

  auto aWindow = Handle(Wasm_Window)::DownCast(view->Window());
  if (eventType == EMSCRIPTEN_EVENT_MOUSEMOVE ||
      eventType == EMSCRIPTEN_EVENT_MOUSEUP) {
    jsUpdateBoundingClientRect();
    EmscriptenMouseEvent anEvent = *event;
    anEvent.targetX -= jsGetBoundingClientLeft();
    anEvent.targetY -= jsGetBoundingClientLeft();
    aWindow->ProcessMouseEvent(*this, eventType, &anEvent);
    return EM_FALSE;
  }
  return aWindow->ProcessMouseEvent(*this, eventType, event) ? EM_TRUE
                                                             : EM_FALSE;
}

EM_BOOL
StaircaseViewController::onWheelEvent(int eventType,
                                      EmscriptenWheelEvent const *event) {
  if (view.IsNull() || eventType != EMSCRIPTEN_EVENT_WHEEL) { return EM_FALSE; }

  Handle(Wasm_Window) aWindow = Handle(Wasm_Window)::DownCast(view->Window());
  return aWindow->ProcessWheelEvent(*this, eventType, event) ? EM_TRUE
                                                             : EM_FALSE;
}

EM_BOOL
StaircaseViewController::onTouchEvent(int eventType,
                                      EmscriptenTouchEvent const *event) {
  if (view.IsNull()) { return EM_FALSE; }

  Handle(Wasm_Window) aWindow = Handle(Wasm_Window)::DownCast(view->Window());
  return aWindow->ProcessTouchEvent(*this, eventType, event) ? EM_TRUE
                                                             : EM_FALSE;
}

EM_BOOL
StaircaseViewController::onFocusEvent(int eventType,
                                      EmscriptenFocusEvent const *event) {
  if (view.IsNull() ||
      (eventType != EMSCRIPTEN_EVENT_FOCUS &&
       eventType != EMSCRIPTEN_EVENT_FOCUSIN // about to receive focus
       && eventType != EMSCRIPTEN_EVENT_FOCUSOUT)) {
    return EM_FALSE;
  }

  Handle(Wasm_Window) aWindow = Handle(Wasm_Window)::DownCast(view->Window());
  return aWindow->ProcessFocusEvent(*this, eventType, event) ? EM_TRUE
                                                             : EM_FALSE;
}

EM_BOOL StaircaseViewController::onKeyDownEvent (int eventType, const EmscriptenKeyboardEvent* event)
{
  if (view.IsNull()
   || eventType != EMSCRIPTEN_EVENT_KEYDOWN)
  {
    return EM_FALSE;
  }

  Handle(Wasm_Window) aWindow = Handle(Wasm_Window)::DownCast (view->Window());
  return aWindow->ProcessKeyEvent (*this, eventType, event) ? EM_TRUE : EM_FALSE;
}

EM_BOOL StaircaseViewController::onKeyUpEvent (int eventType, const EmscriptenKeyboardEvent* event)
{
  if (view.IsNull()
   || eventType != EMSCRIPTEN_EVENT_KEYUP)
  {
    return EM_FALSE;
  }

  Handle(Wasm_Window) aWindow = Handle(Wasm_Window)::DownCast (view->Window());
  return aWindow->ProcessKeyEvent (*this, eventType, event) ? EM_TRUE : EM_FALSE;
}

EM_BOOL
StaircaseViewController::onResizeEvent(int eventType,
                                       EmscriptenUiEvent const *event) {

  (void)eventType;
  (void)event;
  if (view.IsNull()) { return EM_FALSE; }

  auto aWindow = Handle(Wasm_Window)::DownCast(view->Window());
  Graphic3d_Vec2i newSize;
  aWindow->DoResize();
  aWindow->Size(newSize.x(), newSize.y());
  float const newRatio = emscripten_get_device_pixel_ratio();
  bool shouldRedraw = false;
  if (newSize != windowSize) {
    windowSize = newSize;
    shouldRedraw = true;
  }
  if (newRatio != devicePixelRatio) {
    devicePixelRatio = newRatio;

    shouldRedraw = true;
  }
  if (shouldRedraw) {
    view->MustBeResized();
    view->Invalidate();
    view->Redraw();
  }
  return EM_TRUE;
}

char const *StaircaseViewController::getCanvasTag() {
  prefixedCanvasId = "#" + canvasId;
  return prefixedCanvasId.c_str();
}

Handle(V3d_View) StaircaseViewController::getView() const { return view; }

void StaircaseViewController::setView(Handle(V3d_View) const &view) {
  this->view = view;
}
Handle(AIS_InteractiveContext) StaircaseViewController::getAISContext() const {
  return aisContext;
}

void StaircaseViewController::setAISContext(Handle(AIS_InteractiveContext)
                                                const &aisContext) {
  this->aisContext = aisContext;
}
Graphic3d_Vec2i const &StaircaseViewController::getWindowSize() const {
  return windowSize;
}
