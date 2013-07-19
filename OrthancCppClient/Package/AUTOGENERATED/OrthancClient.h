/**
 * Laaw - Lightweight, Automated API Wrapper
 * Copyright (C) 2010-2013 Jomago - Alain Mazy, Benjamin Golinvaux,
 * Sebastien Jodogne
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


/**
 * @file
 **/

#pragma once

#include <stdexcept>
#include <memory>
#include <string>
#include <string.h>

#if defined(_WIN32)

/********************************************************************
 ** This is the Windows-specific section
 ********************************************************************/

#include <windows.h>

/* cf. http://sourceforge.net/p/predef/wiki/Architectures/ */
#ifdef _M_X64
/* 64 bits target */
#define LAAW_ORTHANC_CLIENT_CALL_CONV  __fastcall
#define LAAW_ORTHANC_CLIENT_CALL_DECORATION(Name, StdCallSuffix) Name
#define LAAW_ORTHANC_CLIENT_DEFAULT_PATH  "OrthancClient_Windows64.dll"
#else
/* 32 bits target */
#define LAAW_ORTHANC_CLIENT_CALL_CONV  __stdcall
#define LAAW_ORTHANC_CLIENT_CALL_DECORATION(Name, StdCallSuffix) "_" Name "@" StdCallSuffix
#define LAAW_ORTHANC_CLIENT_DEFAULT_PATH  "OrthancClient_Windows32.dll"
#endif

#define LAAW_ORTHANC_CLIENT_HANDLE_TYPE  HINSTANCE
#define LAAW_ORTHANC_CLIENT_HANDLE_NULL  0
#define LAAW_ORTHANC_CLIENT_FUNCTION_TYPE  FARPROC
#define LAAW_ORTHANC_CLIENT_LOADER(path) LoadLibraryA(path)
#define LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle, name, decoration) GetProcAddress(handle, LAAW_ORTHANC_CLIENT_CALL_DECORATION(name, decoration))
#define LAAW_ORTHANC_CLIENT_CLOSER(handle) FreeLibrary(handle)


/********************************************************************
 ** This is the Linux-specific section
 ********************************************************************/

#elif defined (__linux)

#include <stdlib.h>
#include <dlfcn.h>

/* cf. http://sourceforge.net/p/predef/wiki/Architectures/ */
#ifdef __amd64__
#define LAAW_ORTHANC_CLIENT_DEFAULT_PATH  "libOrthancClient_Linux64.so.1.0"
#else
#define LAAW_ORTHANC_CLIENT_DEFAULT_PATH  "libOrthancClient_Linux32.so.1.0"
#endif

#define LAAW_ORTHANC_CLIENT_CALL_CONV
#define LAAW_ORTHANC_CLIENT_HANDLE_TYPE  void*
#define LAAW_ORTHANC_CLIENT_HANDLE_NULL  NULL
#define LAAW_ORTHANC_CLIENT_FUNCTION_TYPE  intptr_t
#define LAAW_ORTHANC_CLIENT_LOADER(path) dlopen(path, RTLD_LAZY)
#define LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle, name, decoration) (LAAW_ORTHANC_CLIENT_FUNCTION_TYPE) dlsym(handle, name)
#define LAAW_ORTHANC_CLIENT_CLOSER(handle) dlclose(handle)


#else
#error Please support your platform here
#endif


/********************************************************************
 ** Definition of the integer types
 ********************************************************************/

#ifndef LAAW_INT8  // Only define the integer types once

#if defined(__GNUC__)

// Under GCC (including MinGW), the stdint.h standard header is used.

#include <stdint.h>

#define LAAW_INT8  int8_t
#define LAAW_UINT8  uint8_t
#define LAAW_INT16  int16_t
#define LAAW_UINT16  uint16_t
#define LAAW_INT32  int32_t
#define LAAW_UINT32  uint32_t
#define LAAW_INT64  int64_t
#define LAAW_UINT64  uint64_t

#elif defined(_MSC_VER)

// Under Visual Studio, it is required to define the various integer
// types by hand.

#if (_MSC_VER < 1300)
typedef signed char       LAAW_INT8;
typedef signed short      LAAW_INT16;
typedef signed int        LAAW_INT32;
typedef unsigned char     LAAW_UINT8;
typedef unsigned short    LAAW_UINT16;
typedef unsigned int      LAAW_UINT32;
#else
typedef signed __int8     LAAW_INT8;
typedef signed __int16    LAAW_INT16;
typedef signed __int32    LAAW_INT32;
typedef unsigned __int8   LAAW_UINT8;
typedef unsigned __int16  LAAW_UINT16;
typedef unsigned __int32  LAAW_UINT32;
#endif

typedef signed __int64   LAAW_INT64;
typedef unsigned __int64 LAAW_UINT64;

#else
#error "Please support your compiler here"
#endif

#endif





/********************************************************************
 ** This is a shared section between Windows and Linux
 ********************************************************************/

namespace OrthancClient { 
/**
 * @brief Exception class that is thrown by the functions of this shared library.
 **/
class OrthancClientException : public std::exception
  {
  private:
    std::string message_;

  public:
    /**
     * @brief Constructs an exception.
     * @param message The error message.
     **/
    OrthancClientException(std::string message) : message_(message) 
    {
    }

    ~OrthancClientException() throw()
    {
    }

    /**
     * @brief Get the error message associated with this exception.
     * @returns The error message.
     **/
    const std::string& What() const throw()
    {
      return message_; 
    }
};
}


namespace OrthancClient { namespace Internals { 
/**
 * This internal class implements a Singleton design pattern that will
 * store a reference to the shared library handle, together with a
 * pointer to each function in the shared library.
 **/
class Library
  {
  private:
    LAAW_ORTHANC_CLIENT_HANDLE_TYPE  handle_;
    LAAW_ORTHANC_CLIENT_FUNCTION_TYPE  functionsIndex_[53 + 1];



    void Load(const char* sharedLibraryPath)
    {

      if (handle_ != LAAW_ORTHANC_CLIENT_HANDLE_NULL)
      {
        // Do nothing if the library is already loaded
        return;
      }

      /* Setup the path to the default shared library if not provided */
      if (sharedLibraryPath == NULL)
      {
        sharedLibraryPath = LAAW_ORTHANC_CLIENT_DEFAULT_PATH;
      }

      /* Load the shared library */
      handle_ = LAAW_ORTHANC_CLIENT_LOADER(sharedLibraryPath);


      if (handle_ == LAAW_ORTHANC_CLIENT_HANDLE_NULL)
      {
        throw ::OrthancClient::OrthancClientException("Error loading shared library");
      }

      LoadFunctions();
    }

    inline void LoadFunctions();

    void FreeString(char* str)
    {
      typedef void (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (char*);
      Function function = (Function) GetFunction(53);
      function(str);
    }

    Library()
    {
      handle_ = LAAW_ORTHANC_CLIENT_HANDLE_NULL;
    }

    ~Library()
    {
      Finalize();
    }

  public:
    LAAW_ORTHANC_CLIENT_FUNCTION_TYPE  GetFunction(unsigned int index)
    {
      /**
       * If the library has not been manually initialized by a call to
       * ::OrthancClient::Initialize(), it is loaded from
       * the default location (lazy initialization).
       **/
      if (handle_ == NULL)
      {
        Load(NULL);
      }

      return functionsIndex_[index];
    }

    void ThrowExceptionIfNeeded(char* message)
    {
      if (message != NULL)
      {
        std::string tmp(message);
        FreeString(message);
        throw ::OrthancClient::OrthancClientException(tmp);
      }
    }

    static inline Library& GetInstance()
    {
      /**
       * This function defines a "static variable" inside a "static
       * inline method" of a class.  This ensures that a single
       * instance of this variable will be used across all the
       * compilation modules of the software.
       * http://stackoverflow.com/a/1389403/881731
       **/

      static Library singleton;
      return singleton;
    }

    static void Initialize(const char* sharedLibraryPath)
    {
      GetInstance().Load(sharedLibraryPath);
    }

    void Finalize()
    {
      if (handle_ != LAAW_ORTHANC_CLIENT_HANDLE_NULL)
      {
        LAAW_ORTHANC_CLIENT_CLOSER(handle_);
        handle_ = LAAW_ORTHANC_CLIENT_HANDLE_NULL;
      }
    }
};
}}




namespace OrthancClient { 
/*!
 * \addtogroup Initialization Initialization of the shared library.
 * @{
 */

/**
 * @brief Manually initialize the shared library, using the default library name.
 * 
 * Call this method before using the library to ensure correct
 * behaviour in multi-threaded applications.  This method is also
 * useful to control the time at which the shared library is
 * loaded (e.g. for real-time applications).
 **/
inline void Initialize()
{
  ::OrthancClient::Internals::Library::Initialize(NULL);
}

/**
 * @brief Manually initialize the shared library.
 * 
 * Call this method before using the library to ensure correct
 * behaviour in multi-threaded applications.  This method is also
 * useful to control the time at which the shared library is
 * loaded (e.g. for real-time applications).
 *
 * @param sharedLibraryPath The path to the shared library that
 * contains the module.
 **/
inline void Initialize(const std::string& sharedLibraryPath)
{
  ::OrthancClient::Internals::Library::Initialize(sharedLibraryPath.c_str());
}

/**
 * @brief Manually finalize the shared library.
 * 
 * Calling explicitly this function is not mandatory. It is useful to
 * force the release of the resources acquired by the shared library,
 * or to manually control the order in which the global variables get
 * deleted.
 **/
inline void Finalize()
{
  ::OrthancClient::Internals::Library::GetInstance().Finalize();
}


/**
 * @}
 */
}



void ::OrthancClient::Internals::Library::LoadFunctions()
{
  typedef const char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) ();
  Function getVersion = (Function) LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_GetVersion", "0");
  if (getVersion == NULL)
  {
    throw ::OrthancClient::OrthancClientException("Unable to get the library version");
  }

  /**
   * It is assumed that the API does not change when the revision
   * number (MAJOR.MINOR.REVISION) changes.
   **/
  if (strcmp(getVersion(), "1.0"))
  {
    throw ::OrthancClient::OrthancClientException("Mismatch between the C++ header and the library version");
  }

  functionsIndex_[53] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_FreeString", "4");
  functionsIndex_[3] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_557aee7b61817292a0f31269d3c35db7", "8");
  functionsIndex_[4] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_0b8dff0ce67f10954a49b059e348837e", "8");
  functionsIndex_[5] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_e05097c153f676e5a5ee54dcfc78256f", "4");
  functionsIndex_[6] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_e840242bf58d17d3c1d722da09ce88e0", "8");
  functionsIndex_[7] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_c9af31433001b5dfc012a552dc6d0050", "8");
  functionsIndex_[8] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_3fba4d6b818180a44cd1cae6046334dc", "12");
  functionsIndex_[0] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_1f1acb322ea4d0aad65172824607673c", "8");
  functionsIndex_[1] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_f3fd272e4636f6a531aabb72ee01cd5b", "16");
  functionsIndex_[2] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_12d3de0a96e9efb11136a9811bb9ed38", "4");
  functionsIndex_[11] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_f756172daf04516eec3a566adabb4335", "4");
  functionsIndex_[12] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_ddb68763ec902a97d579666a73a20118", "8");
  functionsIndex_[13] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_fba3c68b4be7558dbc65f7ce1ab57d63", "12");
  functionsIndex_[14] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_b4ca99d958f843493e58d1ef967340e1", "8");
  functionsIndex_[15] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_78d5cc76d282437b6f93ec3b82c35701", "16");
  functionsIndex_[9] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_6cf0d7268667f9b0aa4511bacf184919", "12");
  functionsIndex_[10] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_7d81cd502ee27e859735d0ea7112b5a1", "4");
  functionsIndex_[18] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_48a2a1a9d68c047e22bfba23014643d2", "4");
  functionsIndex_[19] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_152cb1b704c053d24b0dab7461ba6ea3", "8");
  functionsIndex_[20] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_852bf8296ca21c5fde5ec565cc10721d", "8");
  functionsIndex_[21] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_efd04574e0779faa83df1f2d8f9888db", "12");
  functionsIndex_[22] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_736247ff5e8036dac38163da6f666ed5", "8");
  functionsIndex_[23] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_d82d2598a7a73f4b6fcc0c09c25b08ca", "8");
  functionsIndex_[24] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_eee03f337ec81d9f1783cd41e5238757", "8");
  functionsIndex_[25] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_006f08237bd7611636fc721baebfb4c5", "8");
  functionsIndex_[26] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_b794f5cd3dad7d7b575dd1fd902afdd0", "8");
  functionsIndex_[27] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_8ee2e50dd9df8f66a3c1766090dd03ab", "8");
  functionsIndex_[28] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_046aed35bbe4751691f4c34cc249a61d", "8");
  functionsIndex_[29] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_88134b978f9acb2aecdadf54aeab3c64", "16");
  functionsIndex_[30] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_4dcc7a0fd025efba251ac6e9b701c2c5", "28");
  functionsIndex_[16] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_193599b9e345384fcdfcd47c29c55342", "12");
  functionsIndex_[17] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_7c97f17063a357d38c5fab1136ad12a0", "4");
  functionsIndex_[33] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_e65b20b7e0170b67544cd6664a4639b7", "4");
  functionsIndex_[34] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_470e981b0e41f17231ba0ae6f3033321", "8");
  functionsIndex_[35] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_04cefd138b6ea15ad909858f2a0a8f05", "12");
  functionsIndex_[36] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_aee5b1f6f0c082f2c3b0986f9f6a18c7", "8");
  functionsIndex_[37] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_93965682bace75491413e1f0b8d5a654", "16");
  functionsIndex_[31] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_b01c6003238eb46c8db5dc823d7ca678", "12");
  functionsIndex_[32] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_0147007fb99bad8cd95a139ec8795376", "4");
  functionsIndex_[40] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_236ee8b403bc99535a8a4695c0cd45cb", "8");
  functionsIndex_[41] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_2a437b7aba6bb01e81113835be8f0146", "8");
  functionsIndex_[42] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_2bcbcb850934ae0bb4c6f0cc940e6cda", "8");
  functionsIndex_[43] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_8d415c3a78a48e7e61d9fd24e7c79484", "12");
  functionsIndex_[44] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_70d2f8398bbc63b5f792b69b4ad5fecb", "12");
  functionsIndex_[45] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_1729a067d902771517388eedd7346b23", "12");
  functionsIndex_[46] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_72e2aeee66cd3abd8ab7e987321c3745", "8");
  functionsIndex_[47] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_1ea3df5a1ac1a1a687fe7325adddb6f0", "8");
  functionsIndex_[48] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_99b4f370e4f532d8b763e2cb49db92f8", "8");
  functionsIndex_[49] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_c41c742b68617f1c0590577a0a5ebc0c", "8");
  functionsIndex_[50] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_142dd2feba0fc1d262bbd0baeb441a8b", "8");
  functionsIndex_[51] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_5f5c9f81a4dff8daa6c359f1d0488fef", "12");
  functionsIndex_[52] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_c0f494b80d4ff8b232df7a75baa0700a", "4");
  functionsIndex_[38] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_6c5ad02f91b583e29cebd0bd319ce21d", "12");
  functionsIndex_[39] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_4068241c44a9c1367fe0e57be523f207", "4");
  
  /* Check whether the functions were properly loaded */
  for (unsigned int i = 0; i <= 53; i++)
  {
    if (functionsIndex_[i] == (LAAW_ORTHANC_CLIENT_FUNCTION_TYPE) NULL)
    {
      throw ::OrthancClient::OrthancClientException("Unable to load the functions of the shared library");
    }
  }
}
namespace OrthancClient
{
  class OrthancConnection;
}

namespace OrthancClient
{
  class Patient;
}

namespace OrthancClient
{
  class Series;
}

namespace OrthancClient
{
  class Study;
}

namespace OrthancClient
{
  class Instance;
}

namespace Orthanc
{
  enum PixelFormat
  {
    PixelFormat_SignedGrayscale16 = 3,
    PixelFormat_RGB24 = 0,
    PixelFormat_Grayscale8 = 1,
    PixelFormat_Grayscale16 = 2
  };
}

namespace Orthanc
{
  enum ImageExtractionMode
  {
    ImageExtractionMode_Int16 = 3,
    ImageExtractionMode_Preview = 0,
    ImageExtractionMode_UInt8 = 1,
    ImageExtractionMode_UInt16 = 2
  };
}

namespace OrthancClient
{
  class OrthancConnection
  {
  private:
    bool isReference_;
  public:
    void* pimpl_;
    OrthancConnection(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
    OrthancConnection(const OrthancConnection& other) { *this = other; }
    void operator= (const OrthancConnection& other) { if (!other.isReference_) throw ::OrthancClient::OrthancClientException("Cannot copy a non-reference object"); pimpl_ = other.pimpl_; isReference_ = true;  }
    inline OrthancConnection(const ::std::string& orthancUrl);
    inline OrthancConnection(const ::std::string& orthancUrl, const ::std::string& username, const ::std::string& password);
    inline ~OrthancConnection();
    inline LAAW_UINT32 GetThreadCount() const;
    inline void SetThreadCount(LAAW_UINT32 threadCount);
    inline void Reload();
    inline ::std::string GetOrthancUrl() const;
    inline LAAW_UINT32 GetPatientCount();
    inline ::OrthancClient::Patient GetPatient(LAAW_UINT32 index);
  };
}

namespace OrthancClient
{
  class Patient
  {
  private:
    bool isReference_;
  public:
    void* pimpl_;
    Patient(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
    Patient(const Patient& other) { *this = other; }
    void operator= (const Patient& other) { if (!other.isReference_) throw ::OrthancClient::OrthancClientException("Cannot copy a non-reference object"); pimpl_ = other.pimpl_; isReference_ = true;  }
    inline Patient(::OrthancClient::OrthancConnection& connection, const ::std::string& id);
    inline ~Patient();
    inline void Reload();
    inline LAAW_UINT32 GetStudyCount();
    inline ::OrthancClient::Study GetStudy(LAAW_UINT32 index);
    inline ::std::string GetId() const;
    inline ::std::string GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const;
  };
}

namespace OrthancClient
{
  class Series
  {
  private:
    bool isReference_;
  public:
    void* pimpl_;
    Series(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
    Series(const Series& other) { *this = other; }
    void operator= (const Series& other) { if (!other.isReference_) throw ::OrthancClient::OrthancClientException("Cannot copy a non-reference object"); pimpl_ = other.pimpl_; isReference_ = true;  }
    inline Series(::OrthancClient::OrthancConnection& connection, const ::std::string& id);
    inline ~Series();
    inline void Reload();
    inline bool Is3DImage();
    inline LAAW_UINT32 GetInstanceCount();
    inline ::OrthancClient::Instance GetInstance(LAAW_UINT32 index);
    inline ::std::string GetId() const;
    inline ::std::string GetUrl() const;
    inline LAAW_UINT32 GetWidth();
    inline LAAW_UINT32 GetHeight();
    inline float GetVoxelSizeX();
    inline float GetVoxelSizeY();
    inline float GetVoxelSizeZ();
    inline ::std::string GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const;
    inline void Load3DImage(void* target, ::Orthanc::PixelFormat format, LAAW_INT64 lineStride, LAAW_INT64 stackStride);
  };
}

namespace OrthancClient
{
  class Study
  {
  private:
    bool isReference_;
  public:
    void* pimpl_;
    Study(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
    Study(const Study& other) { *this = other; }
    void operator= (const Study& other) { if (!other.isReference_) throw ::OrthancClient::OrthancClientException("Cannot copy a non-reference object"); pimpl_ = other.pimpl_; isReference_ = true;  }
    inline Study(::OrthancClient::OrthancConnection& connection, const ::std::string& id);
    inline ~Study();
    inline void Reload();
    inline LAAW_UINT32 GetSeriesCount();
    inline ::OrthancClient::Series GetSeries(LAAW_UINT32 index);
    inline ::std::string GetId() const;
    inline ::std::string GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const;
  };
}

namespace OrthancClient
{
  class Instance
  {
  private:
    bool isReference_;
  public:
    void* pimpl_;
    Instance(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
    Instance(const Instance& other) { *this = other; }
    void operator= (const Instance& other) { if (!other.isReference_) throw ::OrthancClient::OrthancClientException("Cannot copy a non-reference object"); pimpl_ = other.pimpl_; isReference_ = true;  }
    inline Instance(::OrthancClient::OrthancConnection& connection, const ::std::string& id);
    inline ~Instance();
    inline ::std::string GetId() const;
    inline void SetImageExtractionMode(::Orthanc::ImageExtractionMode mode);
    inline ::Orthanc::ImageExtractionMode GetImageExtractionMode() const;
    inline ::std::string GetTagAsString(const ::std::string& tag) const;
    inline float GetTagAsFloat(const ::std::string& tag) const;
    inline LAAW_INT32 GetTagAsInt(const ::std::string& tag) const;
    inline LAAW_UINT32 GetWidth();
    inline LAAW_UINT32 GetHeight();
    inline LAAW_UINT32 GetPitch();
    inline ::Orthanc::PixelFormat GetPixelFormat();
    inline const void* GetBuffer();
    inline const void* GetBuffer(LAAW_UINT32 y);
    inline void DiscardImage();
  };
}

namespace OrthancClient
{
  inline OrthancConnection::OrthancConnection(const ::std::string& orthancUrl)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(0);
    char* error = function(&pimpl_, orthancUrl.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline OrthancConnection::OrthancConnection(const ::std::string& orthancUrl, const ::std::string& username, const ::std::string& password)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, const char*, const char*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(1);
    char* error = function(&pimpl_, orthancUrl.c_str(), username.c_str(), password.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Destructs the object.
  *
  * Destructs the object.
  *
  **/
  inline OrthancConnection::~OrthancConnection()
  {
    if (isReference_) return;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(2);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline LAAW_UINT32 OrthancConnection::GetThreadCount() const
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(3);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline void OrthancConnection::SetThreadCount(LAAW_UINT32 threadCount)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(4);
    char* error = function(pimpl_, threadCount);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline void OrthancConnection::Reload()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(5);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline ::std::string OrthancConnection::GetOrthancUrl() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(6);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  inline LAAW_UINT32 OrthancConnection::GetPatientCount()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(7);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline ::OrthancClient::Patient OrthancConnection::GetPatient(LAAW_UINT32 index)
  {
    void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(8);
    char* error = function(pimpl_, &result_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return ::OrthancClient::Patient(result_);
  }
}

namespace OrthancClient
{
  inline Patient::Patient(::OrthancClient::OrthancConnection& connection, const ::std::string& id)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(9);
    char* error = function(&pimpl_, connection.pimpl_, id.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Destructs the object.
  *
  * Destructs the object.
  *
  **/
  inline Patient::~Patient()
  {
    if (isReference_) return;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(10);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline void Patient::Reload()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(11);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline LAAW_UINT32 Patient::GetStudyCount()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(12);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline ::OrthancClient::Study Patient::GetStudy(LAAW_UINT32 index)
  {
    void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(13);
    char* error = function(pimpl_, &result_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return ::OrthancClient::Study(result_);
  }
  inline ::std::string Patient::GetId() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(14);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  inline ::std::string Patient::GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**, const char*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(15);
    char* error = function(pimpl_, &result_, tag.c_str(), defaultValue.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
}

namespace OrthancClient
{
  inline Series::Series(::OrthancClient::OrthancConnection& connection, const ::std::string& id)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(16);
    char* error = function(&pimpl_, connection.pimpl_, id.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Destructs the object.
  *
  * Destructs the object.
  *
  **/
  inline Series::~Series()
  {
    if (isReference_) return;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(17);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline void Series::Reload()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(18);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline bool Series::Is3DImage()
  {
    LAAW_INT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_INT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(19);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_ != 0;
  }
  inline LAAW_UINT32 Series::GetInstanceCount()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(20);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline ::OrthancClient::Instance Series::GetInstance(LAAW_UINT32 index)
  {
    void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(21);
    char* error = function(pimpl_, &result_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return ::OrthancClient::Instance(result_);
  }
  inline ::std::string Series::GetId() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(22);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  inline ::std::string Series::GetUrl() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(23);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  inline LAAW_UINT32 Series::GetWidth()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(24);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline LAAW_UINT32 Series::GetHeight()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(25);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline float Series::GetVoxelSizeX()
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, float*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(26);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline float Series::GetVoxelSizeY()
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, float*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(27);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline float Series::GetVoxelSizeZ()
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, float*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(28);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline ::std::string Series::GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**, const char*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(29);
    char* error = function(pimpl_, &result_, tag.c_str(), defaultValue.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  inline void Series::Load3DImage(void* target, ::Orthanc::PixelFormat format, LAAW_INT64 lineStride, LAAW_INT64 stackStride)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void*, LAAW_INT32, LAAW_INT64, LAAW_INT64);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(30);
    char* error = function(pimpl_, target, format, lineStride, stackStride);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
}

namespace OrthancClient
{
  inline Study::Study(::OrthancClient::OrthancConnection& connection, const ::std::string& id)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(31);
    char* error = function(&pimpl_, connection.pimpl_, id.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Destructs the object.
  *
  * Destructs the object.
  *
  **/
  inline Study::~Study()
  {
    if (isReference_) return;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(32);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline void Study::Reload()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(33);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline LAAW_UINT32 Study::GetSeriesCount()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(34);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline ::OrthancClient::Series Study::GetSeries(LAAW_UINT32 index)
  {
    void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(35);
    char* error = function(pimpl_, &result_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return ::OrthancClient::Series(result_);
  }
  inline ::std::string Study::GetId() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(36);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  inline ::std::string Study::GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**, const char*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(37);
    char* error = function(pimpl_, &result_, tag.c_str(), defaultValue.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
}

namespace OrthancClient
{
  inline Instance::Instance(::OrthancClient::OrthancConnection& connection, const ::std::string& id)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(38);
    char* error = function(&pimpl_, connection.pimpl_, id.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Destructs the object.
  *
  * Destructs the object.
  *
  **/
  inline Instance::~Instance()
  {
    if (isReference_) return;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(39);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline ::std::string Instance::GetId() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(40);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  inline void Instance::SetImageExtractionMode(::Orthanc::ImageExtractionMode mode)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_INT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(41);
    char* error = function(pimpl_, mode);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  inline ::Orthanc::ImageExtractionMode Instance::GetImageExtractionMode() const
  {
    LAAW_INT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, LAAW_INT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(42);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return static_cast< ::Orthanc::ImageExtractionMode >(result_);
  }
  inline ::std::string Instance::GetTagAsString(const ::std::string& tag) const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(43);
    char* error = function(pimpl_, &result_, tag.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  inline float Instance::GetTagAsFloat(const ::std::string& tag) const
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, float*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(44);
    char* error = function(pimpl_, &result_, tag.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline LAAW_INT32 Instance::GetTagAsInt(const ::std::string& tag) const
  {
    LAAW_INT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, LAAW_INT32*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(45);
    char* error = function(pimpl_, &result_, tag.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline LAAW_UINT32 Instance::GetWidth()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(46);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline LAAW_UINT32 Instance::GetHeight()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(47);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline LAAW_UINT32 Instance::GetPitch()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(48);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  inline ::Orthanc::PixelFormat Instance::GetPixelFormat()
  {
    LAAW_INT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_INT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(49);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return static_cast< ::Orthanc::PixelFormat >(result_);
  }
  inline const void* Instance::GetBuffer()
  {
    const void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, const void**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(50);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return reinterpret_cast< const void* >(result_);
  }
  inline const void* Instance::GetBuffer(LAAW_UINT32 y)
  {
    const void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, const void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(51);
    char* error = function(pimpl_, &result_, y);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return reinterpret_cast< const void* >(result_);
  }
  inline void Instance::DiscardImage()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(52);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
}

