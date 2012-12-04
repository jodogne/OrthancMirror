#include "gtest/gtest.h"

#include "../Core/OrthancException.h"
#include "../Core/Compression/ZipWriter.h"
#include "../Core/Compression/HierarchicalZipWriter.h"
#include "../Core/Toolbox.h"


using namespace Orthanc;

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





namespace Orthanc
{
  // The namespace is necessary
  // http://code.google.com/p/googletest/wiki/AdvancedGuide#Private_Class_Members

  TEST(HierarchicalZipWriter, Index)
  {
    HierarchicalZipWriter::Index i;
    ASSERT_EQ("hello", i.CreateFile("hello"));
    ASSERT_EQ("hello-2", i.CreateFile("hello"));
    ASSERT_EQ("coucou", i.CreateFile("coucou"));
    ASSERT_EQ("hello-3", i.CreateFile("hello"));

    i.CreateDirectory("coucou");

    ASSERT_EQ("coucou-2/world", i.CreateFile("world"));
    ASSERT_EQ("coucou-2/world-2", i.CreateFile("world"));

    i.CreateDirectory("world");
  
    ASSERT_EQ("coucou-2/world-3/hello", i.CreateFile("hello"));
    ASSERT_EQ("coucou-2/world-3/hello-2", i.CreateFile("hello"));

    i.CloseDirectory();

    ASSERT_EQ("coucou-2/world-4", i.CreateFile("world"));

    i.CloseDirectory();

    ASSERT_EQ("coucou-3", i.CreateFile("coucou"));

    ASSERT_THROW(i.CloseDirectory(), OrthancException);
  }


  TEST(HierarchicalZipWriter, Filenames)
  {
    ASSERT_EQ("trE hell", HierarchicalZipWriter::Index::KeepAlphanumeric("    ÊtrE hellô  "));
    ASSERT_EQ("Hello world", HierarchicalZipWriter::Index::KeepAlphanumeric("    Hel^^lo  \t  <world>  "));
  }
}


TEST(HierarchicalZipWriter, Basic)
{
  static const std::string SPACES = "                             ";

  HierarchicalZipWriter w("hello2.zip");

  w.SetCompressionLevel(0);

  // Inside "/"
  w.CreateFile("hello");
  w.Write(SPACES + "hello\n");
  w.CreateFile("hello");
  w.Write(SPACES + "hello-2\n");
  w.CreateDirectory("hello");

  // Inside "/hello-3"
  w.CreateFile("hello");
  w.Write(SPACES + "hello\n");
  w.CreateDirectory("hello");

  w.SetCompressionLevel(9);

  // Inside "/hello-3/hello-2"
  w.CreateFile("hello");
  w.Write(SPACES + "hello\n");
  w.CreateFile("hello");
  w.Write(SPACES + "hello-2\n");
  w.CloseDirectory();

  // Inside "/hello-3"
  w.CreateFile("hello");
  w.Write(SPACES + "hello-3\n");

  /**

     TO CHECK THE CONTENT OF THE "hello2.zip" FILE:

     # unzip -v hello2.zip 

     => There must be 6 files. The first 3 files must have a negative
     compression ratio.

  **/
}
