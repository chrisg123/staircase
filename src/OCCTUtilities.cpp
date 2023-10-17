#include "OCCTUtilities.hpp"
#include <GLES2/gl2.h>
#include <OpenGl_GraphicDriver.hxx>
#include <Wasm_Window.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <emscripten.h>
#include <opencascade/AIS_Shape.hxx>
#include <opencascade/STEPCAFControl_Reader.hxx>
#include <opencascade/TDF_ChildIterator.hxx>
#include <opencascade/TDataStd_Name.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFDoc_ShapeTool.hxx>

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
    Handle(XCAFApp_Application) app, std::string stepFileStr,
    std::function<void(std::optional<Handle(TDocStd_Document)>)> callback) {

  auto aNewDoc = [&]() -> Handle(TDocStd_Document) {
    Handle(TDocStd_Document) aDoc;
    app->NewDocument("MDTV-XCAF", aDoc);
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

std::vector<TopoDS_Shape> getShapesFromDoc(Handle(TDocStd_Document)
                                               const aDoc) {
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

void renderStepFile(AppContext &context) {
  Handle(TDocStd_Document) aDoc = context.currentlyViewingDoc;
  Handle(AIS_InteractiveContext) aisContext = context.getAISContext();
  Handle(V3d_View) aView = context.getView();
  if (aDoc.IsNull() || context.getAISContext().IsNull()) {
    std::cerr << "No document or AIS context available to render." << std::endl;
    return;
  }

  if (!context.stepFileLoaded) {
    std::vector<TopoDS_Shape> shapes = getShapesFromDoc(aDoc);
    {
      Timer timer = Timer("Populate aisContext with shapes;");
      for (auto const &shape : shapes) {
        Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
        aisContext->SetDisplayMode(aisShape, AIS_SHADED_MODE, Standard_True);
        aisContext->Display(aisShape, Standard_True);
      }
    }

    context.stepFileLoaded = true;
  }

  if (context.shouldRotate) {
    static double angle = 0.0;
    angle += 0.01;
    if (angle >= 2 * M_PI) { angle = 0.0; }

    gp_Dir aDir(sin(angle), cos(angle), aView->Camera()->Direction().Z());
    aView->Camera()->SetDirection(aDir);
    aView->Redraw();
  }
}
