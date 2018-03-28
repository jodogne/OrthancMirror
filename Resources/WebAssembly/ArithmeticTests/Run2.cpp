#include <iostream>
#include <emscripten/emscripten.h>

extern "C"
{
  void EMSCRIPTEN_KEEPALIVE Run2()
  {
    // This stuff is not properly discovered by DCMTK 3.6.2 configuration scripts
    std::cerr << std::endl << std::endl;
    std::cerr << "/**" << std::endl;
    std::cerr << "#define SIZEOF_CHAR " << sizeof(char) << std::endl;
    std::cerr << "#define SIZEOF_DOUBLE " << sizeof(double) << std::endl;    
    std::cerr << "#define SIZEOF_FLOAT " << sizeof(float) << std::endl;
    std::cerr << "#define SIZEOF_INT " << sizeof(int) << std::endl;
    std::cerr << "#define SIZEOF_LONG " << sizeof(long) << std::endl;
    std::cerr << "#define SIZEOF_SHORT " << sizeof(short) << std::endl;
    std::cerr << "#define SIZEOF_VOID_P " << sizeof(void*) << std::endl;
    std::cerr << "#define C_CHAR_UNSIGNED " << (!std::is_signed<char>()) << std::endl;
    std::cerr << "**/" << std::endl;
    std::cerr << std::endl << std::endl;
  }
}
