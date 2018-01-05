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

#include "../../Core/OrthancException.h"
#include "../Plugins/Engine/PluginsManager.h"

using namespace Orthanc;


#if ORTHANC_ENABLE_PLUGINS == 1

TEST(SharedLibrary, Enumerations)
{
  // The plugin engine cannot work if the size of an enumeration does
  // not correspond to the size of "int32_t"
  ASSERT_EQ(sizeof(int32_t), sizeof(OrthancPluginErrorCode));
}


TEST(SharedLibrary, Basic)
{
#if defined(_WIN32)
  SharedLibrary l("kernel32.dll");
  ASSERT_THROW(l.GetFunction("world"), OrthancException);
  ASSERT_TRUE(l.GetFunction("GetVersionExW") != NULL);
  ASSERT_TRUE(l.HasFunction("GetVersionExW"));
  ASSERT_FALSE(l.HasFunction("world"));

#elif defined(__LSB_VERSION__)
  // For Linux Standard Base, we use a low-level shared library coming
  // with glibc:
  // http://www.linuxfromscratch.org/lfs/view/6.5/chapter06/glibc.html
  SharedLibrary l("libSegFault.so");
  ASSERT_THROW(l.GetFunction("world"), OrthancException);
  ASSERT_TRUE(l.GetFunction("_init") != NULL);
  ASSERT_TRUE(l.HasFunction("_init"));
  ASSERT_FALSE(l.HasFunction("world"));

#elif defined(__linux__) || defined(__FreeBSD_kernel__)
  SharedLibrary l("libdl.so");
  ASSERT_THROW(l.GetFunction("world"), OrthancException);
  ASSERT_TRUE(l.GetFunction("dlopen") != NULL);
  ASSERT_TRUE(l.HasFunction("dlclose"));
  ASSERT_FALSE(l.HasFunction("world"));

#elif defined(__FreeBSD__) || defined(__OpenBSD__)
  // dlopen() in FreeBSD/OpenBSD is supplied by libc, libc.so is
  // a ldscript, so we can't actually use it. Use thread
  // library instead - if it works - dlopen() is good.
  SharedLibrary l("libpthread.so");
  ASSERT_THROW(l.GetFunction("world"), OrthancException);
  ASSERT_TRUE(l.GetFunction("pthread_create") != NULL);
  ASSERT_TRUE(l.HasFunction("pthread_cancel"));
  ASSERT_FALSE(l.HasFunction("world"));

#elif defined(__APPLE__) && defined(__MACH__)
  SharedLibrary l("libdl.dylib");
  ASSERT_THROW(l.GetFunction("world"), OrthancException);
  ASSERT_TRUE(l.GetFunction("dlopen") != NULL);
  ASSERT_TRUE(l.HasFunction("dlclose"));
  ASSERT_FALSE(l.HasFunction("world"));

#else
#error Support your platform here
#endif
}

#endif
