#include <emscripten.h>
#include <iostream>
#include <opencascade/Standard_Version.hxx>
#include "EmbeddedStepFile.h"

int main() {
  std::string occt_version_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);

  std::cout << occt_version_str << std::endl;

  EM_ASM({ document.getElementById('version').innerHTML = UTF8ToString($0); },
         occt_version_str.c_str());

  EM_ASM({ document.getElementById('stepText').innerHTML = UTF8ToString($0); },
         embeddedStepFile.c_str());

  return 0;
}
