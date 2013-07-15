/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "Toolbox.h"

#include "OrthancException.h"

#include <stdint.h>
#include <string.h>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <algorithm>
#include <ctype.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#include <limits.h>      /* PATH_MAX */
#endif

#if defined(__linux)
#include <limits.h>      /* PATH_MAX */
#include <signal.h>
#include <unistd.h>
#endif

#if BOOST_HAS_LOCALE == 1
#include <boost/locale.hpp>
#else
#include <iconv.h>
#endif

#include "../Resources/md5/md5.h"
#include "../Resources/base64/base64.h"
#include "../Resources/sha1/sha1.h"


#if BOOST_HAS_LOCALE == 0
namespace
{
  class IconvRabi
  {
  private:
    iconv_t context_;

  public:
    IconvRabi(const char* tocode, const char* fromcode)
    {
      context_ = iconv_open(tocode, fromcode);
      if (!context_)
      {
        throw Orthanc::OrthancException("Unknown code page");
      }
    }
    
    ~IconvRabi()
    {
      iconv_close(context_);
    }

    std::string Convert(const std::string& source)
    {
      if (source.size() == 0)
      {
        return "";
      }

      std::string result;
      char* sourcePos = const_cast<char*>(&source[0]);
      size_t sourceLeft = source.size();

      std::vector<char> storage(source.size() + 10);
      
      while (sourceLeft > 0)
      {
        char* tmp = &storage[0];
        size_t outputLeft = storage.size();
        size_t err = iconv(context_, &sourcePos, &sourceLeft, &tmp, &outputLeft);
        if (err < 0)
        {
          throw Orthanc::OrthancException("Bad character in sequence");
        }

        size_t count = storage.size() - outputLeft;
        result += std::string(&storage[0], count);
      }

      return result;
    }
  };
}
#endif


namespace Orthanc
{
  static bool finish;

#if defined(_WIN32)
  static BOOL WINAPI ConsoleControlHandler(DWORD dwCtrlType)
  {
    // http://msdn.microsoft.com/en-us/library/ms683242(v=vs.85).aspx
    finish = true;
    return true;
  }
#else
  static void SignalHandler(int)
  {
    finish = true;
  }
#endif

  void Toolbox::Sleep(uint32_t seconds)
  {
#if defined(_WIN32)
    ::Sleep(static_cast<DWORD>(seconds) * static_cast<DWORD>(1000));
#elif defined(__linux)
    usleep(static_cast<uint64_t>(seconds) * static_cast<uint64_t>(1000000));
#else
#error Support your platform here
#endif
  }

  void Toolbox::USleep(uint64_t microSeconds)
  {
#if defined(_WIN32)
    ::Sleep(static_cast<DWORD>(microSeconds / static_cast<uint64_t>(1000)));
#elif defined(__linux)
    usleep(microSeconds);
#else
#error Support your platform here
#endif
  }


  void Toolbox::ServerBarrier()
  {
#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleControlHandler, true);
#else
    signal(SIGINT, SignalHandler);
    signal(SIGQUIT, SignalHandler);
#endif
  
    finish = false;
    while (!finish)
    {
      USleep(100000);
    }

#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleControlHandler, false);
#else
    signal(SIGINT, NULL);
    signal(SIGQUIT, NULL);
#endif
  }



  void Toolbox::ToUpperCase(std::string& s)
  {
    std::transform(s.begin(), s.end(), s.begin(), toupper);
  }


  void Toolbox::ToLowerCase(std::string& s)
  {
    std::transform(s.begin(), s.end(), s.begin(), tolower);
  }



  void Toolbox::ReadFile(std::string& content,
                         const std::string& path) 
  {
    boost::filesystem::ifstream f;
    f.open(path, std::ifstream::in | std::ios::binary);
    if (!f.good())
    {
      throw OrthancException("Unable to open a file");
    }

    // http://www.cplusplus.com/reference/iostream/istream/tellg/
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    content.resize(size);
    if (size != 0)
    {
      f.read(reinterpret_cast<char*>(&content[0]), size);
    }

    f.close();
  }


  void Toolbox::RemoveFile(const std::string& path)
  {
    if (boost::filesystem::exists(path))
    {
      if (boost::filesystem::is_regular_file(path))
        boost::filesystem::remove(path);
      else
        throw OrthancException("The path is not a regular file: " + path);
    }
  }



  void Toolbox::SplitUriComponents(UriComponents& components,
                                   const std::string& uri)
  {
    static const char URI_SEPARATOR = '/';

    components.clear();

    if (uri.size() == 0 ||
        uri[0] != URI_SEPARATOR)
    {
      throw OrthancException(ErrorCode_UriSyntax);
    }

    // Count the number of slashes in the URI to make an assumption
    // about the number of components in the URI
    unsigned int estimatedSize = 0;
    for (unsigned int i = 0; i < uri.size(); i++)
    {
      if (uri[i] == URI_SEPARATOR)
        estimatedSize++;
    }

    components.reserve(estimatedSize - 1);

    unsigned int start = 1;
    unsigned int end = 1;
    while (end < uri.size())
    {
      // This is the loop invariant
      assert(uri[start - 1] == '/' && (end >= start));

      if (uri[end] == '/')
      {
        components.push_back(std::string(&uri[start], end - start));
        end++;
        start = end;
      }
      else
      {
        end++;
      }
    }

    if (start < uri.size())
    {
      components.push_back(std::string(&uri[start], end - start));
    }

    for (size_t i = 0; i < components.size(); i++)
    {
      if (components[i].size() == 0)
      {
        // Empty component, as in: "/coucou//e"
        throw OrthancException(ErrorCode_UriSyntax);
      }
    }
  }


  bool Toolbox::IsChildUri(const UriComponents& baseUri,
                           const UriComponents& testedUri)
  {
    if (testedUri.size() < baseUri.size())
    {
      return false;
    }

    for (size_t i = 0; i < baseUri.size(); i++)
    {
      if (baseUri[i] != testedUri[i])
        return false;
    }

    return true;
  }


  std::string Toolbox::AutodetectMimeType(const std::string& path)
  {
    std::string contentType;
    size_t lastDot = path.rfind('.');
    size_t lastSlash = path.rfind('/');

    if (lastDot == std::string::npos ||
        (lastSlash != std::string::npos && lastDot < lastSlash))
    {
      // No trailing dot, unable to detect the content type
    }
    else
    {
      const char* extension = &path[lastDot + 1];
    
      // http://en.wikipedia.org/wiki/Mime_types
      // Text types
      if (!strcmp(extension, "txt"))
        contentType = "text/plain";
      else if (!strcmp(extension, "html"))
        contentType = "text/html";
      else if (!strcmp(extension, "xml"))
        contentType = "text/xml";
      else if (!strcmp(extension, "css"))
        contentType = "text/css";

      // Application types
      else if (!strcmp(extension, "js"))
        contentType = "application/javascript";
      else if (!strcmp(extension, "json"))
        contentType = "application/json";
      else if (!strcmp(extension, "pdf"))
        contentType = "application/pdf";

      // Images types
      else if (!strcmp(extension, "jpg") || !strcmp(extension, "jpeg"))
        contentType = "image/jpeg";
      else if (!strcmp(extension, "gif"))
        contentType = "image/gif";
      else if (!strcmp(extension, "png"))
        contentType = "image/png";
    }

    return contentType;
  }


  std::string Toolbox::FlattenUri(const UriComponents& components,
                                  size_t fromLevel)
  {
    if (components.size() <= fromLevel)
    {
      return "/";
    }
    else
    {
      std::string r;

      for (size_t i = fromLevel; i < components.size(); i++)
      {
        r += "/" + components[i];
      }

      return r;
    }
  }



  uint64_t Toolbox::GetFileSize(const std::string& path)
  {
    try
    {
      return static_cast<uint64_t>(boost::filesystem::file_size(path));
    }
    catch (boost::filesystem::filesystem_error)
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }
  }


  static char GetHexadecimalCharacter(uint8_t value)
  {
    assert(value < 16);

    if (value < 10)
      return value + '0';
    else
      return (value - 10) + 'a';
  }


  void Toolbox::ComputeMD5(std::string& result,
                           const std::string& data)
  {
    md5_state_s state;
    md5_init(&state);

    if (data.size() > 0)
    {
      md5_append(&state, reinterpret_cast<const md5_byte_t*>(&data[0]), 
                 static_cast<int>(data.size()));
    }

    md5_byte_t actualHash[16];
    md5_finish(&state, actualHash);

    result.resize(32);
    for (unsigned int i = 0; i < 16; i++)
    {
      result[2 * i] = GetHexadecimalCharacter(actualHash[i] / 16);
      result[2 * i + 1] = GetHexadecimalCharacter(actualHash[i] % 16);
    }
  }


  std::string Toolbox::EncodeBase64(const std::string& data)
  {
    return base64_encode(data);
  }

  std::string Toolbox::DecodeBase64(const std::string& data)
  {
    return base64_decode(data);
  }


#if defined(_WIN32)
  std::string Toolbox::GetPathToExecutable()
  {
    // Yes, this is ugly, but there is no simple way to get the 
    // required buffer size, so we use a big constant
    std::vector<char> buffer(32768);
    /*int bytes =*/ GetModuleFileNameA(NULL, &buffer[0], static_cast<DWORD>(buffer.size() - 1));
    return std::string(&buffer[0]);
  }

#elif defined(__linux)
  std::string Toolbox::GetPathToExecutable()
  {
    std::vector<char> buffer(PATH_MAX + 1);
    ssize_t bytes = readlink("/proc/self/exe", &buffer[0], buffer.size() - 1);
    if (bytes == 0)
    {
      throw OrthancException("Unable to get the path to the executable");
    }

    return std::string(&buffer[0]);
  }

#elif defined(__APPLE__) && defined(__MACH__)
  std::string Toolbox::GetPathToExecutable()
  {
    char pathbuf[PATH_MAX + 1];
    unsigned int  bufsize = static_cast<int>(sizeof(pathbuf));

    _NSGetExecutablePath( pathbuf, &bufsize);

    return std::string(pathbuf);
  }

#else
#error Support your platform here
#endif


  std::string Toolbox::GetDirectoryOfExecutable()
  {
    boost::filesystem::path p(GetPathToExecutable());
    return p.parent_path().string();
  }


  std::string Toolbox::ConvertToUtf8(const std::string& source,
                                     const char* fromEncoding)
  {
#if BOOST_HAS_LOCALE == 1
    try
    {
      return boost::locale::conv::to_utf<char>(source, fromEncoding);
    }
    catch (std::runtime_error&)
    {
      // Bad input string or bad encoding
      return ConvertToAscii(source);
    }
#else
    IconvRabi iconv("UTF-8", fromEncoding);
    try
    {
      return iconv.Convert(source);
    }
    catch (OrthancException)
    {
      return ConvertToAscii(source);
    }
#endif
  }


  std::string Toolbox::ConvertToAscii(const std::string& source)
  {
    std::string result;

    result.reserve(source.size());
    for (size_t i = 0; i < source.size(); i++)
    {
      if (source[i] < 128 && source[i] >= 0 && !iscntrl(source[i]))
      {
        result.push_back(source[i]);
      }
    }

    return result;
  }

  void Toolbox::ComputeSHA1(std::string& result,
                            const std::string& data)
  {
    SHA1 sha1;
    if (data.size() > 0)
    {
      sha1.Input(&data[0], data.size());
    }

    unsigned digest[5];

    // Sanity check for the memory layout: A SHA-1 digest is 160 bits wide
    assert(sizeof(unsigned) == 4 && sizeof(digest) == (160 / 8)); 
    
    if (sha1.Result(digest))
    {
      result.resize(8 * 5 + 4);
      sprintf(&result[0], "%08x-%08x-%08x-%08x-%08x",
              digest[0],
              digest[1],
              digest[2],
              digest[3],
              digest[4]);
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }

  bool Toolbox::IsSHA1(const std::string& str)
  {
    if (str.size() != 44)
    {
      return false;
    }

    for (unsigned int i = 0; i < 44; i++)
    {
      if (i == 8 ||
          i == 17 ||
          i == 26 ||
          i == 35)
      {
        if (str[i] != '-')
          return false;
      }
      else
      {
        if (!isalnum(str[i]))
          return false;
      }
    }

    return true;
  }

  std::string Toolbox::GetNowIsoString()
  {
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    return boost::posix_time::to_iso_string(now);
  }

  std::string Toolbox::StripSpaces(const std::string& source)
  {
    size_t first = 0;

    while (first < source.length() &&
           isspace(source[first]))
    {
      first++;
    }

    if (first == source.length())
    {
      // String containing only spaces
      return "";
    }

    size_t last = source.length();
    while (last > first &&
           isspace(source[last - 1]))
    {
      last--;
    }          
    
    assert(first <= last);
    return source.substr(first, last - first);
  }


  static char Hex2Dec(char c)
  {
    return ((c >= '0' && c <= '9') ? c - '0' :
            ((c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - 'A' + 10));
  }

  void Toolbox::UrlDecode(std::string& s)
  {
    // http://en.wikipedia.org/wiki/Percent-encoding
    // http://www.w3schools.com/tags/ref_urlencode.asp
    // http://stackoverflow.com/questions/154536/encode-decode-urls-in-c

    if (s.size() == 0)
    {
      return;
    }

    size_t source = 0;
    size_t target = 0;

    while (source < s.size())
    {
      if (s[source] == '%' &&
          source + 2 < s.size() &&
          isalnum(s[source + 1]) &&
          isalnum(s[source + 2]))
      {
        s[target] = (Hex2Dec(s[source + 1]) << 4) | Hex2Dec(s[source + 2]);
        source += 3;
        target += 1;
      }
      else
      {
        if (s[source] == '+')
          s[target] = ' ';
        else
          s[target] = s[source];

        source++;
        target++;
      }
    }

    s.resize(target);
  }


  Endianness Toolbox::DetectEndianness()
  {
    // http://sourceforge.net/p/predef/wiki/Endianness/

    uint8_t buffer[4];

    buffer[0] = 0x00;
    buffer[1] = 0x01;
    buffer[2] = 0x02;
    buffer[3] = 0x03;

    switch (*((uint32_t *)buffer)) 
    {
      case 0x00010203: 
        return Endianness_Big;

      case 0x03020100: 
        return Endianness_Little;
        
      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }
}
