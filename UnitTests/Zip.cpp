#include "gtest/gtest.h"

#include "../Core/OrthancException.h"
#include "../Core/Compression/ZipWriter.h"


TEST(ZipWriter, Basic)
{
  Orthanc::ZipWriter w;
  w.SetOutputPath("hello.zip");
  w.Open();
  w.CreateFileInZip("world/hello");
  w.Write("Hello world");
}


TEST(ZipWriter, Exceptions)
{
  Orthanc::ZipWriter w;
  ASSERT_THROW(w.Open(), Orthanc::OrthancException);
  w.SetOutputPath("hello.zip");
  w.Open();
  ASSERT_THROW(w.Write("hello world"), Orthanc::OrthancException);
}
