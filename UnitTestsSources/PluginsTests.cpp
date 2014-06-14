/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include <glog/logging.h>

#include "../Plugins/Engine/SharedLibrary.h"
#include "../Plugins/OrthancCPlugin/OrthancCPlugin.h"

using namespace Orthanc;

TEST(SharedLibrary, Basic)
{
#if defined(_WIN32)
#error Support your platform here

#elif defined(__linux)
  SharedLibrary l("libdl.so");
  ASSERT_THROW(l.GetFunction("world"), OrthancException);
  ASSERT_TRUE(l.GetFunction("dlopen") != NULL);
  ASSERT_TRUE(l.HasFunction("dlclose"));
  ASSERT_FALSE(l.HasFunction("world"));

#else
#error Support your platform here
#endif

}



static void LogError(const char* str)
{
  LOG(ERROR) << str;
}

static void LogWarning(const char* str)
{
  LOG(WARNING) << str;
}

static void LogInfo(const char* str)
{
  LOG(INFO) << str;
}

static int32_t InvokeService(const char* serviceName,
                             const void* serviceParameters)
{
  return 0;
}



TEST(SharedLibrary, Development)
{
#if defined(_WIN32)
#error Support your platform here

#elif defined(__linux)
  SharedLibrary l("./libPluginTest.so");
  ASSERT_TRUE(l.HasFunction("OrthancPluginFinalize"));
  ASSERT_TRUE(l.HasFunction("OrthancPluginInitialize"));

  OrthancPluginContext context;
  context.orthancVersion = ORTHANC_VERSION;
  context.InvokeService = InvokeService;
  context.LogError = LogError;
  context.LogWarning = LogWarning;
  context.LogInfo = LogInfo;

  typedef void (*Finalize) ();
  typedef int32_t (*Initialize) (const OrthancPluginContext*);

  /**
   * gcc would complain about "ISO C++ forbids casting between
   * pointer-to-function and pointer-to-object" without the trick
   * below, that is known as "the POSIX.1-2003 (Technical Corrigendum
   * 1) workaround". See the man page of "dlsym()".
   * http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
   * http://stackoverflow.com/a/14543811/881731
   **/

  Finalize finalize;
  *(void **) (&finalize) = l.GetFunction("OrthancPluginFinalize");
  assert(finalize != NULL);

  Initialize initialize;
  *(void **) (&initialize) = l.GetFunction("OrthancPluginInitialize");
  assert(initialize != NULL);

  initialize(&context);
  finalize();

#else
#error Support your platform here
#endif

}
