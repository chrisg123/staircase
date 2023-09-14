#include <emscripten.h>
#include <iostream>
#include <opencascade/Standard_Version.hxx>

int main() {
  std::string occt_version_str =
      "OCCT Version: " + std::string(OCC_VERSION_COMPLETE);

  std::cout << occt_version_str << std::endl;

  EM_ASM({ document.getElementById('version').innerHTML = UTF8ToString($0); },
         occt_version_str.c_str());

  return 0;
}
