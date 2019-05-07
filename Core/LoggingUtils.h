#include <sstream>
#include <iostream>

namespace Orthanc
{
  namespace Logging
  {

    /**
      std::streambuf subclass used in FunctionCallingStream
    */
    template<typename T>
    class FuncStreamBuf : public std::stringbuf
    {
    public:
      FuncStreamBuf(T func) : func_(func) {}

      virtual int sync()
      {
        std::string text = this->str();
        const char* buf = text.c_str();
        func_(buf);
        this->str("");
        return 0;
      }
    private:
      T func_;
    };
  }
}