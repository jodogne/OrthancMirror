/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#if ORTHANC_UNIT_TESTS_LINK_FRAMEWORK == 1
// Must be the first to be sure to use the Orthanc framework shared library
#  include <OrthancFramework.h>
#endif

#include <gtest/gtest.h>

#include "../Sources/Toolbox.h"
#include "../Sources/OrthancException.h"
#include "../Sources/HttpServer/BufferHttpSender.h"
#include "../Sources/HttpServer/HttpStreamTranscoder.h"
#include "../Sources/Compression/ZlibCompressor.h"
#include "../Sources/Compression/GzipCompressor.h"

#if ORTHANC_SANDBOXED != 1
#  include "../Sources/HttpServer/FilesystemHttpSender.h"
#  include "../Sources/SystemToolbox.h"
#endif


using namespace Orthanc;


TEST(Gzip, Basic)
{
  std::string s = "Hello world";
 
  std::string compressed;
  GzipCompressor c;
  ASSERT_FALSE(c.HasPrefixWithUncompressedSize());
  IBufferCompressor::Compress(compressed, c, s);

  std::string uncompressed;
  IBufferCompressor::Uncompress(uncompressed, c, compressed);
  ASSERT_EQ(s.size(), uncompressed.size());
  ASSERT_EQ(0, memcmp(&s[0], &uncompressed[0], s.size()));
}


TEST(Gzip, Empty)
{
  std::string s;
 
  std::string compressed;
  GzipCompressor c;
  ASSERT_FALSE(c.HasPrefixWithUncompressedSize());
  c.SetPrefixWithUncompressedSize(false);
  IBufferCompressor::Compress(compressed, c, s);

  std::string uncompressed;
  IBufferCompressor::Uncompress(uncompressed, c, compressed);
  ASSERT_TRUE(uncompressed.empty());
}


TEST(Gzip, BasicWithPrefix)
{
  std::string s = "Hello world";
 
  std::string compressed;
  GzipCompressor c;
  c.SetPrefixWithUncompressedSize(true);
  ASSERT_TRUE(c.HasPrefixWithUncompressedSize());
  IBufferCompressor::Compress(compressed, c, s);

  std::string uncompressed;
  IBufferCompressor::Uncompress(uncompressed, c, compressed);
  ASSERT_EQ(s.size(), uncompressed.size());
  ASSERT_EQ(0, memcmp(&s[0], &uncompressed[0], s.size()));
}


TEST(Gzip, EmptyWithPrefix)
{
  std::string s;
 
  std::string compressed;
  GzipCompressor c;
  c.SetPrefixWithUncompressedSize(true);
  ASSERT_TRUE(c.HasPrefixWithUncompressedSize());
  IBufferCompressor::Compress(compressed, c, s);

  std::string uncompressed;
  IBufferCompressor::Uncompress(uncompressed, c, compressed);
  ASSERT_TRUE(uncompressed.empty());
}


TEST(Zlib, Basic)
{
  std::string s = Toolbox::GenerateUuid();
  s = s + s + s + s;
 
  std::string compressed;
  ZlibCompressor c;
  ASSERT_TRUE(c.HasPrefixWithUncompressedSize());
  IBufferCompressor::Compress(compressed, c, s);

  std::string uncompressed;
  IBufferCompressor::Uncompress(uncompressed, c, compressed);
  ASSERT_EQ(s.size(), uncompressed.size());
  ASSERT_EQ(0, memcmp(&s[0], &uncompressed[0], s.size()));
}


TEST(Zlib, Level)
{
  std::string s = Toolbox::GenerateUuid();
  s = s + s + s + s;
 
  std::string compressed, compressed2;
  ZlibCompressor c;
  c.SetCompressionLevel(9);
  IBufferCompressor::Compress(compressed, c, s);

  c.SetCompressionLevel(0);
  IBufferCompressor::Compress(compressed2, c, s);

  ASSERT_TRUE(compressed.size() < compressed2.size());
}


TEST(Zlib, DISABLED_Corrupted)  // Disabled because it may result in a crash
{
  std::string s = Toolbox::GenerateUuid();
  s = s + s + s + s;
 
  std::string compressed;
  ZlibCompressor c;
  IBufferCompressor::Compress(compressed, c, s);

  ASSERT_FALSE(compressed.empty());
  compressed[compressed.size() - 1] = 'a';
  std::string u;

  ASSERT_THROW(IBufferCompressor::Uncompress(u, c, compressed), OrthancException);
}


TEST(Zlib, Empty)
{
  std::string s = "";
 
  std::string compressed, compressed2;
  ZlibCompressor c;
  IBufferCompressor::Compress(compressed, c, s);
  ASSERT_EQ(compressed, compressed2);

  std::string uncompressed;
  IBufferCompressor::Uncompress(uncompressed, c, compressed);
  ASSERT_TRUE(uncompressed.empty());
}


#if ORTHANC_SANDBOXED != 1
static bool ReadAllStream(std::string& result,
                          IHttpStreamAnswer& stream,
                          bool allowGzip = false,
                          bool allowDeflate = false)
{
  stream.SetupHttpCompression(allowGzip, allowDeflate);

  result.resize(static_cast<size_t>(stream.GetContentLength()));

  size_t pos = 0;
  while (stream.ReadNextChunk())
  {
    size_t s = stream.GetChunkSize();
    if (pos + s > result.size())
    {
      return false;
    }

    memcpy(&result[pos], stream.GetChunkContent(), s);
    pos += s;
  }

  return pos == result.size();
}
#endif


#if ORTHANC_SANDBOXED != 1
TEST(BufferHttpSender, Basic)
{
  const std::string s = "Hello world";
  std::string t;

  {
    BufferHttpSender sender;
    sender.SetChunkSize(1);
    ASSERT_TRUE(ReadAllStream(t, sender));
    ASSERT_EQ(0u, t.size());
  }

  for (int cs = 0; cs < 5; cs++)
  {
    BufferHttpSender sender;
    sender.SetChunkSize(cs);
    sender.GetBuffer() = s;
    ASSERT_TRUE(ReadAllStream(t, sender));
    ASSERT_EQ(s, t);
  }
}
#endif


#if ORTHANC_SANDBOXED != 1
TEST(FilesystemHttpSender, Basic)
{
  const std::string& path = "UnitTestsResults/stream";
  const std::string s = "Hello world";
  std::string t;

  {
    SystemToolbox::WriteFile(s, path);
    FilesystemHttpSender sender(path);
    ASSERT_TRUE(ReadAllStream(t, sender));
    ASSERT_EQ(s, t);
  }

  {
    SystemToolbox::WriteFile("", path);
    FilesystemHttpSender sender(path);
    ASSERT_TRUE(ReadAllStream(t, sender));
    ASSERT_EQ(0u, t.size());
  }
}
#endif


#if ORTHANC_SANDBOXED != 1
TEST(HttpStreamTranscoder, Basic)
{
  ZlibCompressor compressor;

  const std::string s = "Hello world " + Toolbox::GenerateUuid();

  std::string t;
  IBufferCompressor::Compress(t, compressor, s);

  for (int cs = 0; cs < 5; cs++)
  {
    BufferHttpSender sender;
    sender.SetChunkSize(cs);
    sender.GetBuffer() = t;
    std::string u;
    ASSERT_TRUE(ReadAllStream(u, sender));

    std::string v;
    IBufferCompressor::Uncompress(v, compressor, u);
    ASSERT_EQ(s, v);
  }

  // Pass-through test, no decompression occurs
  for (int cs = 0; cs < 5; cs++)
  {
    BufferHttpSender sender;
    sender.SetChunkSize(cs);
    sender.GetBuffer() = t;

    HttpStreamTranscoder transcode(sender, CompressionType_None);
    
    std::string u;
    ASSERT_TRUE(ReadAllStream(u, transcode));
    
    ASSERT_EQ(t, u);
  }

  // Pass-through test, decompression occurs
  for (int cs = 0; cs < 5; cs++)
  {
    BufferHttpSender sender;
    sender.SetChunkSize(cs);
    sender.GetBuffer() = t;

    HttpStreamTranscoder transcode(sender, CompressionType_ZlibWithSize);
    
    std::string u;
    ASSERT_TRUE(ReadAllStream(u, transcode, false, false));
    
    ASSERT_EQ(s, u);
  }

  // Pass-through test with zlib, no decompression occurs but deflate is sent
  for (int cs = 0; cs < 16; cs++)
  {
    BufferHttpSender sender;
    sender.SetChunkSize(cs);
    sender.GetBuffer() = t;

    HttpStreamTranscoder transcode(sender, CompressionType_ZlibWithSize);
    
    std::string u;
    ASSERT_TRUE(ReadAllStream(u, transcode, false, true));
    
    ASSERT_EQ(t.size() - sizeof(uint64_t), u.size());
    ASSERT_EQ(t.substr(sizeof(uint64_t)), u);
  }

  for (int cs = 0; cs < 3; cs++)
  {
    BufferHttpSender sender;
    sender.SetChunkSize(cs);

    HttpStreamTranscoder transcode(sender, CompressionType_ZlibWithSize);
    std::string u;
    ASSERT_TRUE(ReadAllStream(u, transcode, false, true));
    
    ASSERT_EQ(0u, u.size());
  }
}
#endif
