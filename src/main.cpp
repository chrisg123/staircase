#include "EmbeddedStepFile.h"
#include <chrono>
#include <emscripten.h>
#include <iostream>
#include <opencascade/STEPCAFControl_Reader.hxx>
#include <opencascade/Standard_Version.hxx>
#include <opencascade/TDF_ChildIterator.hxx>
#include <opencascade/TDataStd_Name.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFApp_Application.hxx>

class Timer {
public:
  Timer(std::string const &timerName)
      : name(timerName), start(std::chrono::high_resolution_clock::now()) {}

  ~Timer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    double seconds = static_cast<double>(duration) / 1e6;
    std::cout << "[TIMER] " << seconds << "s:" << name << std::endl;
  }

private:
  std::string name;
  std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

/**
 * Recursively prints the hierarchy of labels from a TDF_Label tree.
 *
 * @param label The root label to start the traversal from.
 * @param level Indentation level for better readability (default is 0).
 */
void printLabels(TDF_Label const &label, int level = 0) {
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

int main() {
  std::string occt_version_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);

  std::cout << occt_version_str << std::endl;

  EM_ASM({ document.getElementById('version').innerHTML = UTF8ToString($0); },
         occt_version_str.c_str());

  EM_ASM({ document.getElementById('stepText').innerHTML = UTF8ToString($0); },
         embeddedStepFile.c_str());

  Handle(XCAFApp_Application) anApp = XCAFApp_Application::GetApplication();
  Handle(TDocStd_Document) aDoc;

  anApp->NewDocument("MDTV-XCAF", aDoc);

  STEPCAFControl_Reader aStepReader;

  std::istringstream aStepStream(embeddedStepFile);

  IFSelect_ReturnStatus aStatus =
      aStepReader.ReadStream("Embedded STEP Data", aStepStream);

  if (aStatus == IFSelect_RetDone) {
    if (!aStepReader.Transfer(aDoc)) {
      std::cerr << "Error transferring data from STEP to XCAF." << std::endl;
      return 1;
    }

    std::cout << "Embedded STEP Data successfully read." << std::endl;
    std::cout << "Document structure:" << std::endl;
    printLabels(aDoc->Main());

  } else {
    std::cerr << "Error reading STEP file." << std::endl;
    return 1;
  }
  return 0;
}
