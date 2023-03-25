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


#include "PrecompiledHeaders.h"
#include "SharedLibrary.h"

#include "Logging.h"
#include "OrthancException.h"

#include <boost/filesystem.hpp>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <dlfcn.h>
#else
#error Support your platform here
#endif

namespace Orthanc
{
  SharedLibrary::SharedLibrary(const std::string& path) : 
    path_(path), 
    handle_(NULL)
  {
#if defined(_WIN32)
    handle_ = ::LoadLibraryA(path_.c_str());
    if (handle_ == NULL)
    {
      LOG(ERROR) << "LoadLibrary(" << path_ << ") failed: Error " << ::GetLastError();

      if (::GetLastError() == ERROR_BAD_EXE_FORMAT &&
          sizeof(void*) == 4)
      {
        throw OrthancException(ErrorCode_SharedLibrary,
                               "You are most probably trying to load a 64bit plugin into a 32bit version of Orthanc");
      }
      else if (::GetLastError() == ERROR_BAD_EXE_FORMAT &&
               sizeof(void*) == 8)
      {
        throw OrthancException(ErrorCode_SharedLibrary,
                               "You are most probably trying to load a 32bit plugin into a 64bit version of Orthanc");
      }
      else
      {
        throw OrthancException(ErrorCode_SharedLibrary);
      }
    }

#elif defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__) || defined(__OpenBSD__)
   
    /**
     * "RTLD_LOCAL" is the default, and is only present to be
     * explicit. "RTLD_DEEPBIND" was added in Orthanc 1.6.0, in order
     * to avoid crashes while loading plugins from the LSB binaries of
     * the Orthanc core.
     *
     * BUT this had no effect, and this results in a crash if loading
     * the Python 2.7 plugin => We disabled it again in Orthanc 1.6.1.
     **/
    
#if 0 // && defined(RTLD_DEEPBIND)  // This is a GNU extension
    // Disabled in Orthanc 1.6.1
    handle_ = ::dlopen(path_.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
#else
    handle_ = ::dlopen(path_.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif

    if (handle_ == NULL) 
    {
      std::string explanation;
      const char *tmp = ::dlerror();
      if (tmp)
      {
        explanation = ": Error " + std::string(tmp);
      }

      LOG(ERROR) << "dlopen(" << path_ << ") failed" << explanation;
      throw OrthancException(ErrorCode_SharedLibrary);
    }

#else
#error Support your platform here
#endif   
  }

  SharedLibrary::~SharedLibrary()
  {
    if (handle_)
    {
#if defined(_WIN32)
      ::FreeLibrary((HMODULE)handle_);
#elif defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__) || defined(__OpenBSD__)
      ::dlclose(handle_);
#else
#error Support your platform here
#endif
    }
  }


  const std::string &SharedLibrary::GetPath() const
  {
    return path_;
  }


  SharedLibrary::FunctionPointer SharedLibrary::GetFunctionInternal(const std::string& name)
  {
    if (!handle_)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

#if defined(_WIN32)
    return ::GetProcAddress((HMODULE)handle_, name.c_str());
#elif defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    return ::dlsym(handle_, name.c_str());
#else
#error Support your platform here
#endif
  }


  SharedLibrary::FunctionPointer SharedLibrary::GetFunction(const std::string& name)
  {
    SharedLibrary::FunctionPointer result = GetFunctionInternal(name);
  
    if (result == NULL)
    {
      throw OrthancException(
        ErrorCode_SharedLibrary,
        "Shared library does not expose function \"" + name + "\"");
    }
    else
    {
      return result;
    }
  }


  bool SharedLibrary::HasFunction(const std::string& name)
  {
    return GetFunctionInternal(name) != NULL;
  }
}
