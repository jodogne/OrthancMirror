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

#include "../Sources/Logging.h"
#include "../Sources/OrthancException.h"

#include <boost/regex.hpp>
#include <sstream>


static std::stringstream testErrorStream;
void TestError(const char* message)
{
  testErrorStream << message;
}

static std::stringstream testWarningStream;
void TestWarning(const char* message)
{
  testWarningStream << message;
}

static std::stringstream testInfoStream;
void TestInfo(const char* message)
{
  testInfoStream << message;
}

/**
   Extracts the log line payload

   "E0423 16:55:43.001194 LoggingTests.cpp:102] Foo bar?\n"
   -->
   "Foo bar"

   If the log line cannot be matched, the function returns false.
*/

#define EOLSTRING "\n"

static bool GetLogLinePayload(std::string& payload,
                              const std::string& logLine)
{
  const char* regexStr = "[A-Z][0-9]{4} [0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{6} "
    "[a-zA-Z\\.\\-_]+:[0-9]+\\] (.*)" EOLSTRING "$";

  boost::regex regexObj(regexStr);

  //std::stringstream regexSStr;
  //regexSStr << "E[0-9]{4} [0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{6} "
  //  "[a-zA-Z\\.\\-_]+:[0-9]+\\](.*)\r\n$";
  //std::string regexStr = regexSStr.str();
  boost::regex pattern(regexStr);
  boost::cmatch what;
  if (regex_match(logLine.c_str(), what, regexObj))
  {
    payload = what[1];
    return true;
  }
  else
  {
    return false;
  }
}


namespace
{
  class LoggingMementoScope
  {
  public:
    LoggingMementoScope()
    {
    }
    
    ~LoggingMementoScope()
    {
      Orthanc::Logging::Reset();
    }
  };


  /**
   * std::streambuf subclass used in FunctionCallingStream
   **/
  template<typename T>
  class FuncStreamBuf : public std::stringbuf
  {
  public:
    explicit FuncStreamBuf(T func) : func_(func) {}

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


#if ORTHANC_ENABLE_LOGGING_STDIO == 0
TEST(FuncStreamBuf, BasicTest)
{
  LoggingMementoScope loggingConfiguration;

  Orthanc::Logging::EnableTraceLevel(true);

  typedef void(*LoggingFunctionFunc)(const char*);

  FuncStreamBuf<LoggingFunctionFunc> errorStreamBuf(TestError);
  std::ostream errorStream(&errorStreamBuf);

  FuncStreamBuf<LoggingFunctionFunc> warningStreamBuf(TestWarning);
  std::ostream warningStream(&warningStreamBuf);

  FuncStreamBuf<LoggingFunctionFunc> infoStreamBuf(TestInfo);
  std::ostream infoStream(&infoStreamBuf);

  Orthanc::Logging::SetErrorWarnInfoLoggingStreams(errorStream, warningStream, infoStream);

  {
    const char* text = "E is the set of all sets that do not contain themselves. Does E contain itself?";
    LOG(ERROR) << text;
    std::string logLine = testErrorStream.str();
    testErrorStream.str("");
    testErrorStream.clear();
    std::string payload;
    bool ok = GetLogLinePayload(payload, logLine);
    ASSERT_TRUE(ok);
    ASSERT_STREQ(payload.c_str(), text);
  }

  // make sure loglines do not accumulate
  {
    const char* text = "some more nonsensical babblingiciously stupid gibberish";
    LOG(ERROR) << text;
    std::string logLine = testErrorStream.str();
    testErrorStream.str("");
    testErrorStream.clear();
    std::string payload;
    bool ok = GetLogLinePayload(payload, logLine);
    ASSERT_TRUE(ok);
    ASSERT_STREQ(payload.c_str(), text);
  }

  {
    const char* text = "Trougoudou 53535345345353";
    LOG(WARNING) << text;
    std::string logLine = testWarningStream.str();
    testWarningStream.str("");
    testWarningStream.clear();
    std::string payload;
    bool ok = GetLogLinePayload(payload, logLine);
    ASSERT_TRUE(ok);
    ASSERT_STREQ(payload.c_str(), text);
  }

  {
    const char* text = "Prout 111929";
    LOG(INFO) << text;
    std::string logLine = testInfoStream.str();
    testInfoStream.str("");
    testInfoStream.clear();
    std::string payload;
    bool ok = GetLogLinePayload(payload, logLine);
    ASSERT_TRUE(ok);
    ASSERT_STREQ(payload.c_str(), text);
  }

  Orthanc::Logging::EnableTraceLevel(false);  // Back to normal
}
#endif



TEST(Logging, Categories)
{
  using namespace Orthanc::Logging;
  
  // Unit tests are running in "--verbose" mode (not "--trace")
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_SQLITE));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_SQLITE));

  // Cannot modify categories for ERROR and WARNING
  ASSERT_THROW(SetCategoryEnabled(LogLevel_ERROR, LogCategory_GENERIC, true),
               Orthanc::OrthancException);
  ASSERT_THROW(SetCategoryEnabled(LogLevel_WARNING, LogCategory_GENERIC, false),
               Orthanc::OrthancException);


  EnableInfoLevel(false);
  EnableTraceLevel(false);
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_FALSE(IsInfoLevelEnabled());
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_SQLITE));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_SQLITE));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_SQLITE));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_SQLITE));


  // Test the "category" setters at INFO level
  SetCategoryEnabled(LogLevel_INFO, LogCategory_DICOM, true);
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());   // At least one category is verbose
  
  SetCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC, true);
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());
  
  SetCategoryEnabled(LogLevel_INFO, LogCategory_DICOM, false);
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());  // "GENERIC" is still verbose
  
  SetCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC, false);
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_FALSE(IsInfoLevelEnabled());


  // Test the "category" setters at TRACE level
  SetCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM, true);
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_TRUE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());
  
  SetCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC, true);
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_TRUE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());
  
  SetCategoryEnabled(LogLevel_INFO, LogCategory_DICOM, false);
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_TRUE(IsTraceLevelEnabled());  // "GENERIC" is still at trace level
  ASSERT_TRUE(IsInfoLevelEnabled());
  
  SetCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC, false);
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());
  
  SetCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC, false);
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_FALSE(IsInfoLevelEnabled());



  // Test the "macro" setters
  EnableInfoLevel(true);
  EnableTraceLevel(false);
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_SQLITE));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_SQLITE));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_SQLITE));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_SQLITE));

  EnableInfoLevel(false);
  EnableTraceLevel(true);  // "--trace" implies "--verbose"
  ASSERT_TRUE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_ERROR, LogCategory_SQLITE));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_WARNING, LogCategory_SQLITE));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_SQLITE));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_GENERIC));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_DICOM));
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_SQLITE));



  // Back to normal
  EnableInfoLevel(true);
  EnableTraceLevel(false);
  ASSERT_FALSE(IsTraceLevelEnabled());
  ASSERT_TRUE(IsInfoLevelEnabled());
  ASSERT_TRUE(IsCategoryEnabled(LogLevel_INFO, LogCategory_SQLITE));
  ASSERT_FALSE(IsCategoryEnabled(LogLevel_TRACE, LogCategory_SQLITE));
}


TEST(Logging, Enumerations)
{
  using namespace Orthanc;
  
  Logging::LogCategory c;
  ASSERT_TRUE(Logging::LookupCategory(c, "generic"));  ASSERT_EQ(Logging::LogCategory_GENERIC, c);
  ASSERT_TRUE(Logging::LookupCategory(c, "plugins"));  ASSERT_EQ(Logging::LogCategory_PLUGINS, c);
  ASSERT_TRUE(Logging::LookupCategory(c, "http"));     ASSERT_EQ(Logging::LogCategory_HTTP, c);
  ASSERT_TRUE(Logging::LookupCategory(c, "sqlite"));   ASSERT_EQ(Logging::LogCategory_SQLITE, c);
  ASSERT_TRUE(Logging::LookupCategory(c, "dicom"));    ASSERT_EQ(Logging::LogCategory_DICOM, c);
  ASSERT_TRUE(Logging::LookupCategory(c, "jobs"));     ASSERT_EQ(Logging::LogCategory_JOBS, c);
  ASSERT_TRUE(Logging::LookupCategory(c, "lua"));     ASSERT_EQ(Logging::LogCategory_LUA, c);
  ASSERT_FALSE(Logging::LookupCategory(c, "nope"));

  ASSERT_EQ(7u, Logging::GetCategoriesCount());

  std::set<std::string> s;
  for (size_t i = 0; i < Logging::GetCategoriesCount(); i++)
  {
    ASSERT_TRUE(Logging::LookupCategory(c, Logging::GetCategoryName(i)));
    s.insert(Logging::GetCategoryName(i));
  }

  ASSERT_EQ(7u, s.size());
  ASSERT_TRUE(s.find("generic") != s.end());
  ASSERT_TRUE(s.find("plugins") != s.end());
  ASSERT_TRUE(s.find("http") != s.end());
  ASSERT_TRUE(s.find("sqlite") != s.end());
  ASSERT_TRUE(s.find("dicom") != s.end());
  ASSERT_TRUE(s.find("lua") != s.end());
  ASSERT_TRUE(s.find("jobs") != s.end());

  ASSERT_THROW(Logging::GetCategoryName(Logging::GetCategoriesCount()), OrthancException);

  ASSERT_STREQ("generic", Logging::GetCategoryName(Logging::LogCategory_GENERIC));
  ASSERT_STREQ("plugins", Logging::GetCategoryName(Logging::LogCategory_PLUGINS));
  ASSERT_STREQ("http", Logging::GetCategoryName(Logging::LogCategory_HTTP));
  ASSERT_STREQ("sqlite", Logging::GetCategoryName(Logging::LogCategory_SQLITE));
  ASSERT_STREQ("dicom", Logging::GetCategoryName(Logging::LogCategory_DICOM));
  ASSERT_STREQ("lua", Logging::GetCategoryName(Logging::LogCategory_LUA));
  ASSERT_STREQ("jobs", Logging::GetCategoryName(Logging::LogCategory_JOBS));
}
