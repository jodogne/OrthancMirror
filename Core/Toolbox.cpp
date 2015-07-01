/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "PrecompiledHeaders.h"
#include "Toolbox.h"

#include "OrthancException.h"

#include <string>
#include <stdint.h>
#include <string.h>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/uuid/sha1.hpp>
#include <boost/lexical_cast.hpp>
#include <algorithm>
#include <ctype.h>

#if BOOST_HAS_DATE_TIME == 1
#include <boost/date_time/posix_time/posix_time.hpp>
#endif

#if BOOST_HAS_REGEX == 1
#include <boost/regex.hpp> 
#endif

#if HAVE_GOOGLE_LOG == 1
#include <glog/logging.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <process.h>   // For "_spawnvp()"
#else
#include <unistd.h>    // For "execvp()"
#include <sys/wait.h>  // For "waitpid()"
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#include <limits.h>      /* PATH_MAX */
#endif

#if defined(__linux) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
#include <limits.h>      /* PATH_MAX */
#include <signal.h>
#include <unistd.h>
#endif

#if BOOST_HAS_LOCALE != 1
#error Since version 0.7.6, Orthanc entirely relies on boost::locale
#endif

#include <boost/locale.hpp>

#include "../Resources/ThirdParty/md5/md5.h"
#include "../Resources/ThirdParty/base64/base64.h"


#if defined(_MSC_VER) && (_MSC_VER < 1800)
// Patch for the missing "_strtoll" symbol when compiling with Visual Studio < 2013
extern "C"
{
  int64_t _strtoi64(const char *nptr, char **endptr, int base);
  int64_t strtoll(const char *nptr, char **endptr, int base)
  {
    return _strtoi64(nptr, endptr, base);
  } 
}
#endif


#if ORTHANC_PUGIXML_ENABLED == 1
#include "ChunkedBuffer.h"
#include <pugixml.hpp>
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

  void Toolbox::USleep(uint64_t microSeconds)
  {
#if defined(_WIN32)
    ::Sleep(static_cast<DWORD>(microSeconds / static_cast<uint64_t>(1000)));
#elif defined(__linux) || defined(__APPLE__) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
    usleep(microSeconds);
#else
#error Support your platform here
#endif
  }


  static void ServerBarrierInternal(const bool* stopFlag)
  {
#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleControlHandler, true);
#else
    signal(SIGINT, SignalHandler);
    signal(SIGQUIT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#endif
  
    // Active loop that awakens every 100ms
    finish = false;
    while (!(*stopFlag || finish))
    {
      Toolbox::USleep(100 * 1000);
    }

#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleControlHandler, false);
#else
    signal(SIGINT, NULL);
    signal(SIGQUIT, NULL);
    signal(SIGTERM, NULL);
#endif
  }


  void Toolbox::ServerBarrier(const bool& stopFlag)
  {
    ServerBarrierInternal(&stopFlag);
  }

  void Toolbox::ServerBarrier()
  {
    const bool stopFlag = false;
    ServerBarrierInternal(&stopFlag);
  }


  void Toolbox::ToUpperCase(std::string& s)
  {
    std::transform(s.begin(), s.end(), s.begin(), toupper);
  }


  void Toolbox::ToLowerCase(std::string& s)
  {
    std::transform(s.begin(), s.end(), s.begin(), tolower);
  }


  void Toolbox::ToUpperCase(std::string& result,
                            const std::string& source)
  {
    result = source;
    ToUpperCase(result);
  }

  void Toolbox::ToLowerCase(std::string& result,
                            const std::string& source)
  {
    result = source;
    ToLowerCase(result);
  }


  void Toolbox::ReadFile(std::string& content,
                         const std::string& path) 
  {
    boost::filesystem::ifstream f;
    f.open(path, std::ifstream::in | std::ifstream::binary);
    if (!f.good())
    {
      throw OrthancException(ErrorCode_InexistentFile);
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


  void Toolbox::WriteFile(const std::string& content,
                          const std::string& path)
  {
    boost::filesystem::ofstream f;
    f.open(path, std::ofstream::binary);
    if (!f.good())
    {
      throw OrthancException(ErrorCode_CannotWriteFile);
    }

    if (content.size() != 0)
    {
      f.write(content.c_str(), content.size());
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


  void Toolbox::TruncateUri(UriComponents& target,
                            const UriComponents& source,
                            size_t fromLevel)
  {
    target.clear();

    if (source.size() > fromLevel)
    {
      target.resize(source.size() - fromLevel);

      size_t j = 0;
      for (size_t i = fromLevel; i < source.size(); i++, j++)
      {
        target[j] = source[i];
      }

      assert(j == target.size());
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
    if (data.size() > 0)
    {
      ComputeMD5(result, &data[0], data.size());
    }
    else
    {
      ComputeMD5(result, NULL, 0);
    }
  }


  void Toolbox::ComputeMD5(std::string& result,
                           const void* data,
                           size_t length)
  {
    md5_state_s state;
    md5_init(&state);

    if (length > 0)
    {
      md5_append(&state, 
                 reinterpret_cast<const md5_byte_t*>(data), 
                 static_cast<int>(length));
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


  void Toolbox::EncodeBase64(std::string& result, 
                             const std::string& data)
  {
    result = base64_encode(data);
  }

  void Toolbox::DecodeBase64(std::string& result, 
                             const std::string& data)
  {
    result = base64_decode(data);
  }


#if defined(_WIN32)
  static std::string GetPathToExecutableInternal()
  {
    // Yes, this is ugly, but there is no simple way to get the 
    // required buffer size, so we use a big constant
    std::vector<char> buffer(32768);
    /*int bytes =*/ GetModuleFileNameA(NULL, &buffer[0], static_cast<DWORD>(buffer.size() - 1));
    return std::string(&buffer[0]);
  }

#elif defined(__linux) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
  static std::string GetPathToExecutableInternal()
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
  static std::string GetPathToExecutableInternal()
  {
    char pathbuf[PATH_MAX + 1];
    unsigned int  bufsize = static_cast<int>(sizeof(pathbuf));

    _NSGetExecutablePath( pathbuf, &bufsize);

    return std::string(pathbuf);
  }

#else
#error Support your platform here
#endif


  std::string Toolbox::GetPathToExecutable()
  {
    boost::filesystem::path p(GetPathToExecutableInternal());
    return boost::filesystem::absolute(p).string();
  }


  std::string Toolbox::GetDirectoryOfExecutable()
  {
    boost::filesystem::path p(GetPathToExecutableInternal());
    return boost::filesystem::absolute(p.parent_path()).string();
  }


  std::string Toolbox::ConvertToUtf8(const std::string& source,
                                     const Encoding sourceEncoding)
  {
    const char* encoding;


    // http://bradleyross.users.sourceforge.net/docs/dicom/doc/src-html/org/dcm4che2/data/SpecificCharacterSet.html
    switch (sourceEncoding)
    {
      case Encoding_Utf8:
        // Already in UTF-8: No conversion is required
        return source;

      case Encoding_Ascii:
        return ConvertToAscii(source);

      case Encoding_Latin1:
        encoding = "ISO-8859-1";
        break;

      case Encoding_Latin2:
        encoding = "ISO-8859-2";
        break;

      case Encoding_Latin3:
        encoding = "ISO-8859-3";
        break;

      case Encoding_Latin4:
        encoding = "ISO-8859-4";
        break;

      case Encoding_Latin5:
        encoding = "ISO-8859-9";
        break;

      case Encoding_Cyrillic:
        encoding = "ISO-8859-5";
        break;

      case Encoding_Windows1251:
        encoding = "WINDOWS-1251";
        break;

      case Encoding_Arabic:
        encoding = "ISO-8859-6";
        break;

      case Encoding_Greek:
        encoding = "ISO-8859-7";
        break;

      case Encoding_Hebrew:
        encoding = "ISO-8859-8";
        break;
        
      case Encoding_Japanese:
        encoding = "SHIFT-JIS";
        break;

      case Encoding_Chinese:
        encoding = "GB18030";
        break;

      case Encoding_Thai:
        encoding = "TIS620.2533-0";
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

    try
    {
      return boost::locale::conv::to_utf<char>(source, encoding);
    }
    catch (std::runtime_error&)
    {
      // Bad input string or bad encoding
      return ConvertToAscii(source);
    }
  }


  std::string Toolbox::ConvertToAscii(const std::string& source)
  {
    std::string result;

    result.reserve(source.size() + 1);
    for (size_t i = 0; i < source.size(); i++)
    {
      if (source[i] <= 127 && source[i] >= 0 && !iscntrl(source[i]))
      {
        result.push_back(source[i]);
      }
    }

    return result;
  }

  void Toolbox::ComputeSHA1(std::string& result,
                            const std::string& data)
  {
    boost::uuids::detail::sha1 sha1;

    if (data.size() > 0)
    {
      sha1.process_bytes(&data[0], data.size());
    }

    unsigned int digest[5];

    // Sanity check for the memory layout: A SHA-1 digest is 160 bits wide
    assert(sizeof(unsigned int) == 4 && sizeof(digest) == (160 / 8)); 
    
    sha1.get_digest(digest);

    result.resize(8 * 5 + 4);
    sprintf(&result[0], "%08x-%08x-%08x-%08x-%08x",
            digest[0],
            digest[1],
            digest[2],
            digest[3],
            digest[4]);
  }

  bool Toolbox::IsSHA1(const char* str,
                       size_t size)
  {
    if (size == 0)
    {
      return false;
    }

    const char* start = str;
    const char* end = str + size;

    // Trim the beginning of the string
    while (start < end)
    {
      if (*start == '\0' ||
          isspace(*start))
      {
        start++;
      }
      else
      {
        break;
      }
    }

    // Trim the trailing of the string
    while (start < end)
    {
      if (*(end - 1) == '\0' ||
          isspace(*(end - 1)))
      {
        end--;
      }
      else
      {
        break;
      }
    }

    if (end - start != 44)
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
        if (start[i] != '-')
          return false;
      }
      else
      {
        if (!isalnum(start[i]))
          return false;
      }
    }

    return true;
  }


  bool Toolbox::IsSHA1(const std::string& s)
  {
    if (s.size() == 0)
    {
      return false;
    }
    else
    {
      return IsSHA1(s.c_str(), s.size());
    }
  }


#if BOOST_HAS_DATE_TIME == 1
  std::string Toolbox::GetNowIsoString()
  {
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    return boost::posix_time::to_iso_string(now);
  }
#endif


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


#if BOOST_HAS_REGEX == 1
  std::string Toolbox::WildcardToRegularExpression(const std::string& source)
  {
    // TODO - Speed up this with a regular expression

    std::string result = source;

    // Escape all special characters
    boost::replace_all(result, "\\", "\\\\");
    boost::replace_all(result, "^", "\\^");
    boost::replace_all(result, ".", "\\.");
    boost::replace_all(result, "$", "\\$");
    boost::replace_all(result, "|", "\\|");
    boost::replace_all(result, "(", "\\(");
    boost::replace_all(result, ")", "\\)");
    boost::replace_all(result, "[", "\\[");
    boost::replace_all(result, "]", "\\]");
    boost::replace_all(result, "+", "\\+");
    boost::replace_all(result, "/", "\\/");
    boost::replace_all(result, "{", "\\{");
    boost::replace_all(result, "}", "\\}");

    // Convert wildcards '*' and '?' to their regex equivalents
    boost::replace_all(result, "?", ".");
    boost::replace_all(result, "*", ".*");

    return result;
  }
#endif



  void Toolbox::TokenizeString(std::vector<std::string>& result,
                               const std::string& value,
                               char separator)
  {
    result.clear();

    std::string currentItem;

    for (size_t i = 0; i < value.size(); i++)
    {
      if (value[i] == separator)
      {
        result.push_back(currentItem);
        currentItem.clear();
      }
      else
      {
        currentItem.push_back(value[i]);
      }
    }

    result.push_back(currentItem);
  }


#if BOOST_HAS_REGEX == 1
  void Toolbox::DecodeDataUriScheme(std::string& mime,
                                    std::string& content,
                                    const std::string& source)
  {
    boost::regex pattern("data:([^;]+);base64,([a-zA-Z0-9=+/]*)",
                         boost::regex::icase /* case insensitive search */);

    boost::cmatch what;
    if (regex_match(source.c_str(), what, pattern))
    {
      mime = what[1];
      content = what[2];
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }
#endif


  void Toolbox::MakeDirectory(const std::string& path)
  {
    if (boost::filesystem::exists(path))
    {
      if (!boost::filesystem::is_directory(path))
      {
        throw OrthancException("Cannot create the directory over an existing file: " + path);
      }
    }
    else
    {
      if (!boost::filesystem::create_directories(path))
      {
        throw OrthancException("Unable to create the directory: " + path);
      }
    }
  }


  bool Toolbox::IsExistingFile(const std::string& path)
  {
    return boost::filesystem::exists(path);
  }


#if ORTHANC_PUGIXML_ENABLED == 1
  class ChunkedBufferWriter : public pugi::xml_writer
  {
  private:
    ChunkedBuffer buffer_;

  public:
    virtual void write(const void *data, size_t size)
    {
      if (size > 0)
      {
        buffer_.AddChunk(reinterpret_cast<const char*>(data), size);
      }
    }

    void Flatten(std::string& s)
    {
      buffer_.Flatten(s);
    }
  };


  static void JsonToXmlInternal(pugi::xml_node& target,
                                const Json::Value& source,
                                const std::string& arrayElement)
  {
    // http://jsoncpp.sourceforge.net/value_8h_source.html#l00030

    switch (source.type())
    {
      case Json::nullValue:
      {
        target.append_child(pugi::node_pcdata).set_value("null");
        break;
      }

      case Json::intValue:
      {
        std::string s = boost::lexical_cast<std::string>(source.asInt());
        target.append_child(pugi::node_pcdata).set_value(s.c_str());
        break;
      }

      case Json::uintValue:
      {
        std::string s = boost::lexical_cast<std::string>(source.asUInt());
        target.append_child(pugi::node_pcdata).set_value(s.c_str());
        break;
      }

      case Json::realValue:
      {
        std::string s = boost::lexical_cast<std::string>(source.asFloat());
        target.append_child(pugi::node_pcdata).set_value(s.c_str());
        break;
      }

      case Json::stringValue:
      {
        target.append_child(pugi::node_pcdata).set_value(source.asString().c_str());
        break;
      }

      case Json::booleanValue:
      {
        target.append_child(pugi::node_pcdata).set_value(source.asBool() ? "true" : "false");
        break;
      }

      case Json::arrayValue:
      {
        for (Json::Value::ArrayIndex i = 0; i < source.size(); i++)
        {
          pugi::xml_node node = target.append_child();
          node.set_name(arrayElement.c_str());
          JsonToXmlInternal(node, source[i], arrayElement);
        }
        break;
      }
        
      case Json::objectValue:
      {
        Json::Value::Members members = source.getMemberNames();

        for (size_t i = 0; i < members.size(); i++)
        {
          pugi::xml_node node = target.append_child();
          node.set_name(members[i].c_str());
          JsonToXmlInternal(node, source[members[i]], arrayElement);          
        }

        break;
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void Toolbox::JsonToXml(std::string& target,
                          const Json::Value& source,
                          const std::string& rootElement,
                          const std::string& arrayElement)
  {
    pugi::xml_document doc;

    pugi::xml_node n = doc.append_child(rootElement.c_str());
    JsonToXmlInternal(n, source, arrayElement);

    pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version").set_value("1.0");
    decl.append_attribute("encoding").set_value("utf-8");

    ChunkedBufferWriter writer;
    doc.save(writer, "  ", pugi::format_default, pugi::encoding_utf8);
    writer.Flatten(target);
  }

#endif


  void Toolbox::ExecuteSystemCommand(const std::string& command,
                                     const std::vector<std::string>& arguments)
  {
    // Convert the arguments as a C array
    std::vector<char*>  args(arguments.size() + 2);

    args.front() = const_cast<char*>(command.c_str());

    for (size_t i = 0; i < arguments.size(); i++)
    {
      args[i + 1] = const_cast<char*>(arguments[i].c_str());
    }

    args.back() = NULL;

    int status;

#if defined(_WIN32)
    // http://msdn.microsoft.com/en-us/library/275khfab.aspx
    status = static_cast<int>(_spawnvp(_P_OVERLAY, command.c_str(), &args[0]));

#else
    int pid = fork();

    if (pid == -1)
    {
      // Error in fork()
#if HAVE_GOOGLE_LOG == 1
      LOG(ERROR) << "Cannot fork a child process";
#endif

      throw OrthancException(ErrorCode_SystemCommand);
    }
    else if (pid == 0)
    {
      // Execute the system command in the child process
      execvp(command.c_str(), &args[0]);

      // We should never get here
      _exit(1);
    }
    else
    {
      // Wait for the system command to exit
      waitpid(pid, &status, 0);
    }
#endif

    if (status != 0)
    {
#if HAVE_GOOGLE_LOG == 1
      LOG(ERROR) << "System command failed with status code " << status;
#endif

      throw OrthancException(ErrorCode_SystemCommand);
    }
  }

  
  bool Toolbox::IsInteger(const std::string& str)
  {
    std::string s = StripSpaces(str);

    if (s.size() == 0)
    {
      return false;
    }

    size_t pos = 0;
    if (s[0] == '-')
    {
      if (s.size() == 1)
      {
        return false;
      }

      pos = 1;
    }

    while (pos < s.size())
    {
      if (!isdigit(s[pos]))
      {
        return false;
      }

      pos++;
    }

    return true;
  }


  void Toolbox::CopyJsonWithoutComments(Json::Value& target,
                                        const Json::Value& source)
  {
    switch (source.type())
    {
      case Json::nullValue:
        target = Json::nullValue;
        break;

      case Json::intValue:
        target = source.asInt64();
        break;

      case Json::uintValue:
        target = source.asUInt64();
        break;

      case Json::realValue:
        target = source.asDouble();
        break;

      case Json::stringValue:
        target = source.asString();
        break;

      case Json::booleanValue:
        target = source.asBool();
        break;

      case Json::arrayValue:
      {
        target = Json::arrayValue;
        for (Json::Value::ArrayIndex i = 0; i < source.size(); i++)
        {
          Json::Value& item = target.append(Json::nullValue);
          CopyJsonWithoutComments(item, source[i]);
        }

        break;
      }

      case Json::objectValue:
      {
        target = Json::objectValue;
        Json::Value::Members members = source.getMemberNames();
        for (Json::Value::ArrayIndex i = 0; i < members.size(); i++)
        {
          const std::string item = members[i];
          CopyJsonWithoutComments(target[item], source[item]);
        }

        break;
      }

      default:
        break;
    }
  }


  bool Toolbox::StartsWith(const std::string& str,
                           const std::string& prefix)
  {
    if (str.size() < prefix.size())
    {
      return false;
    }
    else
    {
      return str.compare(0, prefix.size(), prefix) == 0;
    }
  }

}

