#pragma once

#include <stdint.h>
#include <string.h>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class ZipWriter
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    bool hasFileInZip_;
    uint8_t compressionLevel_;
    std::string path_;

  public:
    ZipWriter();

    ~ZipWriter();

    void SetCompressionLevel(uint8_t level);

    uint8_t GetCompressionLevel() const
    {
      return compressionLevel_;
    }
    
    void Open();

    void Close();

    bool IsOpen() const;

    void SetOutputPath(const char* path);

    const std::string& GetOutputPath() const
    {
      return path_;
    }

    void CreateFileInZip(const char* path);

    void Write(const char* data, size_t length);

    void Write(const std::string& data);
  };
}
