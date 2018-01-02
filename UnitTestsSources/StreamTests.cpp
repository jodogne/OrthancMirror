/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include "../Core/SystemToolbox.h"
#include "../Core/SystemToolbox.h"
#include "../Core/Toolbox.h"
#include "../Core/OrthancException.h"
#include "../Core/HttpServer/BufferHttpSender.h"
#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/HttpServer/HttpStreamTranscoder.h"
#include "../Core/Compression/ZlibCompressor.h"
#include "../Core/Compression/GzipCompressor.h"


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
  std::string s = SystemToolbox::GenerateUuid();
  s = s + s + s + s;
 
  std::string compressed, compressed2;
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
  std::string s = SystemToolbox::GenerateUuid();
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
  std::string s = SystemToolbox::GenerateUuid();
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


TEST(HttpStreamTranscoder, Basic)
{
  ZlibCompressor compressor;

  const std::string s = "Hello world " + SystemToolbox::GenerateUuid();

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
