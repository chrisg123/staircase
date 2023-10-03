#include "OCCTUtilities.hpp"
#include <GLES2/gl2.h>
#include <OpenGl_GraphicDriver.hxx>
#include <Wasm_Window.hxx>
#include <emscripten.h>
#include <opencascade/STEPCAFControl_Reader.hxx>
#include <opencascade/TDF_ChildIterator.hxx>
#include <opencascade/TDataStd_Name.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>

void initializeOcctComponents(AppContext &context) {

  float myDevicePixelRatio = emscripten_get_device_pixel_ratio();
  Handle(Aspect_DisplayConnection) aDisp;
  Handle(OpenGl_GraphicDriver) aDriver = new OpenGl_GraphicDriver(aDisp, false);
  aDriver->ChangeOptions().buffersNoSwap = true;
  aDriver->ChangeOptions().buffersOpaqueAlpha = true;

  if (!aDriver->InitContext()) {
    std::cerr << "Error: EGL initialization failed" << std::endl;
    return;
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

  Handle(Wasm_Window) aWindow = new Wasm_Window(context.canvasId.c_str());
  aWindow->Size(context.canvasWidth, context.canvasHeight);

  Handle(Prs3d_TextAspect) myTextStyle = new Prs3d_TextAspect();
  myTextStyle->SetFont(Font_NOF_ASCII_MONO);
  myTextStyle->SetHeight(12);
  myTextStyle->Aspect()->SetColor(Quantity_NOC_GRAY95);
  myTextStyle->Aspect()->SetColorSubTitle(Quantity_NOC_BLACK);
  myTextStyle->Aspect()->SetDisplayType(Aspect_TODT_SHADOW);
  myTextStyle->Aspect()->SetTextFontAspect(Font_FA_Bold);
  myTextStyle->Aspect()->SetTextZoomable(false);
  myTextStyle->SetHorizontalJustification(Graphic3d_HTA_LEFT);
  myTextStyle->SetVerticalJustification(Graphic3d_VTA_BOTTOM);

  Handle(V3d_View) aView = aViewer->CreateView();
  aView->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
  aView->SetImmediateUpdate(false);
  aView->ChangeRenderingParams().IsShadowEnabled = false;
  aView->ChangeRenderingParams().Resolution =
      (unsigned int)(96.0 * myDevicePixelRatio + 0.5);
  aView->ChangeRenderingParams().ToShowStats = true;
  aView->ChangeRenderingParams().StatsTextAspect = myTextStyle->Aspect();
  aView->ChangeRenderingParams().StatsTextHeight = (int)myTextStyle->Height();
  aView->SetWindow(aWindow);

  Handle(AIS_InteractiveContext) aContext = new AIS_InteractiveContext(aViewer);

  context.view = aView;
  context.viewer = aViewer;
  context.aisContext = aContext;
}

std::optional<Handle(TDocStd_Document)>
readInto(std::function<Handle(TDocStd_Document)()> aNewDoc,
         std::istream &fromStream) {

  Handle(TDocStd_Document) aDoc = aNewDoc();
  STEPCAFControl_Reader aStepReader;

  IFSelect_ReturnStatus aStatus =
      aStepReader.ReadStream("Embedded STEP Data", fromStream);

  if (aStatus != IFSelect_RetDone) {
    std::cerr << "Error reading STEP file." << std::endl;
    return std::nullopt;
  }

  bool success = aStepReader.Transfer(aDoc);

  if (!success) {
    std::cerr << "Transfer failed." << std::endl;
    return std::nullopt;
  }

  return aDoc;
}

void printLabels(TDF_Label const &label, int level) {
  for (int i = 0; i < level; ++i) {
    std::cout << "  ";
  }

  Handle(TDataStd_Name) name;
  if (label.FindAttribute(TDataStd_Name::GetID(), name)) {
    TCollection_ExtendedString nameStr = name->Get();
    std::cout << "Label: " << nameStr;
  } else {
    std::cout << "Unnamed Label";
  }
  std::cout << std::endl;

  for (TDF_ChildIterator it(label); it.More(); it.Next()) {
    printLabels(it.Value(), level + 1);
  }
}

void readStepFile(
    AppContext &context, std::string stepFileStr,
    std::function<void(std::optional<Handle(TDocStd_Document)>)> callback) {

  auto aNewDoc = [&]() -> Handle(TDocStd_Document) {
    Handle(TDocStd_Document) aDoc;
    context.getApp()->NewDocument("MDTV-XCAF", aDoc);
    return aDoc;
  };

  std::istringstream fromStream(stepFileStr);

  std::optional<Handle(TDocStd_Document)> docOpt;
  {
    Timer timer = Timer("readInto(aNewDoc, fromStream)");
    docOpt = readInto(aNewDoc, fromStream);
  }

  callback(docOpt);
}

std::vector<TopoDS_Shape> getShapesFromDoc(Handle(TDocStd_Document) const aDoc) {
  std::vector<TopoDS_Shape> shapes;

  TDF_Label mainLabel = aDoc->Main();
  Handle(XCAFDoc_ShapeTool) shapeTool =
      XCAFDoc_DocumentTool::ShapeTool(mainLabel);
  for (TDF_ChildIterator it(mainLabel, Standard_True); it.More(); it.Next()) {
    TDF_Label label = it.Value();
    if (!shapeTool->IsShape(label)) { continue; }
    TopoDS_Shape shape;
    if (!shapeTool->GetShape(label, shape) || shape.IsNull()) {
      std::cerr << "Failed to get shape from label." << std::endl;
      continue;
    }
    shapes.push_back(shape);
  }
  return shapes;
}
