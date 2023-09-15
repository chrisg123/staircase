#include "EmbeddedStepFile.h"
#include <emscripten.h>
#include <iostream>
#include <opencascade/STEPCAFControl_Reader.hxx>
#include <opencascade/Standard_Version.hxx>
#include <opencascade/TDocStd_Document.hxx>
#include <opencascade/XCAFApp_Application.hxx>

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
  } else {
    std::cerr << "Error reading STEP file." << std::endl;
    return 1;
  }
  return 0;
}
