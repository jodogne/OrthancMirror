/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


/**

   This file merges 2 files:
   ${BOOST_SOURCES_DIR}/libs/thread/src/win32/tss_dll.cpp
   ${OPENSSL_SOURCES_DIR}/crypto/dllmain.c

**/

#if defined(_WIN32) || defined(__CYGWIN__)

#include <boost/thread/detail/tss_hooks.hpp>

#include <windows.h>

#include <crypto/cryptlib.h>

#if defined(__BORLANDC__)
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE /*hInstance*/, DWORD dwReason, LPVOID /*lpReserved*/)
#elif defined(_WIN32_WCE)
  extern "C" BOOL WINAPI DllMain(HANDLE /*hInstance*/, DWORD dwReason, LPVOID /*lpReserved*/)
#else
  extern "C" BOOL WINAPI DllMain(HINSTANCE /*hInstance*/, DWORD dwReason, LPVOID /*lpReserved*/)
#endif
{
  switch(dwReason)
  {
    case DLL_PROCESS_ATTACH:
    {
      //OPENSSL_cpuid_setup();  // TODO - Is this necessary?
      boost::on_process_enter();
      boost::on_thread_enter();
      break;
    }

    case DLL_THREAD_ATTACH:
    {
      boost::on_thread_enter();
      break;
    }

    case DLL_THREAD_DETACH:
    {
      OPENSSL_thread_stop();
      boost::on_thread_exit();
      break;
    }

    case DLL_PROCESS_DETACH:
    {
      boost::on_thread_exit();
      boost::on_process_exit();
      break;
    }
  }

  return TRUE;
}

#endif


namespace boost
{
  void tss_cleanup_implemented()
  {
    /*
      This function's sole purpose is to cause a link error in cases where
      automatic tss cleanup is not implemented by Boost.Threads as a
      reminder that user code is responsible for calling the necessary
      functions at the appropriate times (and for implementing an a
      tss_cleanup_implemented() function to eliminate the linker's
      missing symbol error).

      If Boost.Threads later implements automatic tss cleanup in cases
      where it currently doesn't (which is the plan), the duplicate
      symbol error will warn the user that their custom solution is no
      longer needed and can be removed.
    */
  }
}

