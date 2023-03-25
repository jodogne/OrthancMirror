/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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
#include <gtest/gtest.h>

#include "../../OrthancFramework/Sources/Compatibility.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
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
  ASSERT_FALSE(l.HasFunction("world"));

  /**
   * On the Docker image "debian:buster-slim", the "libSegFault.so"
   * library does exist, but does not contain any public symbol:
   * 
   *  $ sudo docker run -i -t --rm --entrypoint=bash debian:buster-slim
   *  # apt-get update && apt-get install -y binutils
   *  # nm -C /lib/x86_64-linux-gnu/libSegFault.so
   *  nm: /lib/x86_64-linux-gnu/libSegFault.so: no symbols
   *
   * As a consequence, this part of the test is disabled since Orthanc
   * 1.5.1, until we locate another shared library that is widely
   * spread. Reference:
   * https://groups.google.com/d/msg/orthanc-users/v-QFzpOzgJY/4Hm5NgxKBwAJ
   **/
  
  //ASSERT_TRUE(l.GetFunction("_init") != NULL);
  //ASSERT_TRUE(l.HasFunction("_init"));
  
#elif defined(__linux__) || defined(__FreeBSD_kernel__)
  /**
   * Since Orthanc 1.10.0, we test the "libdl.so.2" instead of the
   * "libdl.so", as discussed here:
   * https://groups.google.com/g/orthanc-users/c/I5g1fN6MCvg/m/JVdvRyjJAAAJ
   * https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1001305
   * https://salsa.debian.org/med-team/orthanc/-/blob/master/debian/patches/glibc-2.34.patch
   **/

  try
  {
    SharedLibrary l("libdl.so.2");
    ASSERT_THROW(l.GetFunction("world"), OrthancException);
    ASSERT_TRUE(l.GetFunction("dlopen") != NULL);
    ASSERT_TRUE(l.HasFunction("dlclose"));
    ASSERT_FALSE(l.HasFunction("world"));
    return;  // Success
  }
  catch (OrthancException&)
  {
  }
  
  try
  {
    SharedLibrary l("libdl.so"); // Fallback for backward compat
    ASSERT_THROW(l.GetFunction("world"), OrthancException);
    ASSERT_TRUE(l.GetFunction("dlopen") != NULL);
    ASSERT_TRUE(l.HasFunction("dlclose"));
    ASSERT_FALSE(l.HasFunction("world"));
    return;  // Success
  }
  catch (OrthancException&)
  {
  }
  
  try
  {
    SharedLibrary l("libmemusage.so"); // Try another common library
    ASSERT_THROW(l.GetFunction("world"), OrthancException);
    ASSERT_TRUE(l.GetFunction("munmap") != NULL);
    ASSERT_TRUE(l.HasFunction("free"));
    ASSERT_FALSE(l.HasFunction("world"));
    return;  // Success
  }
  catch (OrthancException&)
  {
  }
  
  ASSERT_TRUE(0);

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
