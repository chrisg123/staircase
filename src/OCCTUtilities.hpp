#ifndef OCCTUTILITIES_HPP
#define OCCTUTILITIES_HPP
#include "AppContext.hpp"
#include "staircase.hpp"

std::optional<Handle(TDocStd_Document)>
readInto(std::function<Handle(TDocStd_Document)()> aNewDoc,
         std::istream &fromStream);

/**
 * Recursively prints the hierarchy of labels from a TDF_Label tree.
 *
 * @param label The root label to start the traversal from.
 * @param level Indentation level for better readability (default is 0).
 */
void printLabels(TDF_Label const &label, int level = 0);

void readStepFile(
    Handle(XCAFApp_Application) app, std::string stepFileStr,
    std::function<void(std::optional<Handle(TDocStd_Document)>)> callback);

std::vector<TopoDS_Shape> getShapesFromDoc(Handle(TDocStd_Document) const aDoc);
void renderStepFile(AppContext &context);
#endif
