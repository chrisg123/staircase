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
#include <opencascade/XCAFDoc_ColorTool.hxx>
#include <opencascade/XCAFDoc_ShapeTool.hxx>
#include <unordered_set>
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

std::optional<Quantity_Color> getShapeColor(Handle(TDocStd_Document) const aDoc,
                                       TopoDS_Shape const shape) {
  Handle(XCAFDoc_ColorTool) colorTool =
      XCAFDoc_DocumentTool::ColorTool(aDoc->Main());

  Quantity_Color color;
  if (colorTool->GetColor(shape, XCAFDoc_ColorGen, color) ||
      colorTool->GetColor(shape, XCAFDoc_ColorCurv, color) ||
      colorTool->GetColor(shape, XCAFDoc_ColorSurf, color)) {
    return color;
  }

  return std::nullopt;
}

std::string ShapeEnumToString(TopAbs_ShapeEnum shapeType) {
    switch (shapeType) {
        case TopAbs_COMPOUND:  return "TopAbs_COMPOUND";
        case TopAbs_COMPSOLID: return "TopAbs_COMPSOLID";
        case TopAbs_SOLID:     return "TopAbs_SOLID";
        case TopAbs_SHELL:     return "TopAbs_SHELL";
        case TopAbs_FACE:      return "TopAbs_FACE";
        case TopAbs_WIRE:      return "TopAbs_WIRE";
        case TopAbs_EDGE:      return "TopAbs_EDGE";
        case TopAbs_VERTEX:    return "TopAbs_VERTEX";
        default:               return "Unknown";
    }
}

std::vector<TopoDS_Shape> getShapesFromDoc(Handle(TDocStd_Document)
                                               const aDoc) {
    std::vector<TopoDS_Shape> shapes;
    TDF_Label mainLabel = aDoc->Main();
    Handle(XCAFDoc_ShapeTool) shapeTool =
        XCAFDoc_DocumentTool::ShapeTool(mainLabel);

    std::unordered_set<int> seenLabels;
    for (TDF_ChildIterator it(mainLabel, Standard_True); it.More(); it.Next()) {
      TDF_Label label = it.Value();

      if (seenLabels.find(label.Tag()) != seenLabels.end()) {
        continue;
      }

      if (!shapeTool->IsShape(label)) { continue; }
      TopoDS_Shape shape;
      if (!shapeTool->GetShape(label, shape) || shape.IsNull()) {
        std::cerr << "Failed to get shape from label." << std::endl;
        continue;
      }

      shapes.push_back(shape);
      seenLabels.insert(label.Tag());

      debugOut("[Shape] Label= ", label.Tag(), ", Type= ", shape.ShapeType(), "(",
               ShapeEnumToString(shape.ShapeType()), ")");
    }
    return shapes;
}
