/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include <boost/regex.hpp>
#include <sstream>

#include "../Core/Logging.h"
#include "../Core/LoggingUtils.h"

using namespace Orthanc::Logging;

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
    LoggingMementoScope() : memento_(CreateLoggingMemento()) {}
    ~LoggingMementoScope()
    {
      RestoreLoggingMemento(memento_);
    }
  private:
    LoggingMemento memento_;
  };
}

TEST(FuncStreamBuf, BasicTest)
{
  LoggingMementoScope loggingConfiguration;

  EnableTraceLevel(true);

  typedef void(*LoggingFunctionFunc)(const char*);

  FuncStreamBuf<LoggingFunctionFunc> errorStreamBuf(TestError);
  std::ostream errorStream(&errorStreamBuf);

  FuncStreamBuf<LoggingFunctionFunc> warningStreamBuf(TestWarning);
  std::ostream warningStream(&warningStreamBuf);

  FuncStreamBuf<LoggingFunctionFunc> infoStreamBuf(TestInfo);
  std::ostream infoStream(&infoStreamBuf);

  SetErrorWarnInfoLoggingStreams(&errorStream, &warningStream, &infoStream);

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
}










