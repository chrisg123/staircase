#include <emscripten.h>
#include <iostream>
#include <opencascade/Standard_Version.hxx>

int main() {
  std::cout << "OCCT Version: >> " << OCC_VERSION_COMPLETE << std::endl;

  EM_ASM(
      {
        document.getElementById('version').innerHTML =
            'OCCT Version: ' + UTF8ToString($0);
      },
      OCC_VERSION_COMPLETE);

  return 0;
}
