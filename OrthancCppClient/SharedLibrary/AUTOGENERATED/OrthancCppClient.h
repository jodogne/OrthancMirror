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
#define LAAW_ORTHANC_CLIENT_DEFAULT_PATH  "libOrthancClient.so.0.8"
#else
#define LAAW_ORTHANC_CLIENT_DEFAULT_PATH  "libOrthancClient.so.0.8"
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
    LAAW_ORTHANC_CLIENT_FUNCTION_TYPE  functionsIndex_[63 + 1];



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
      Function function = (Function) GetFunction(63);
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
#if 0
        /**
         * Do not explicitly unload the shared library, as it might
         * interfere with the destruction of static objects declared
         * inside the library (e.g. this is the case of gflags that is
         * internally used by googlelog).
         **/
        LAAW_ORTHANC_CLIENT_CLOSER(handle_);
#endif
        handle_ = LAAW_ORTHANC_CLIENT_HANDLE_NULL;
      }
    }
};
}}


/*!
 * \addtogroup Global Global definitions.
 * @{
 * @}
 */


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


namespace OrthancClient { namespace Internals { 
inline void Library::LoadFunctions()
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
  if (strcmp(getVersion(), "0.8"))
  {
    throw ::OrthancClient::OrthancClientException("Mismatch between the C++ header and the library version");
  }

  functionsIndex_[63] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_FreeString", "4");
  functionsIndex_[3] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_557aee7b61817292a0f31269d3c35db7", "8");
  functionsIndex_[4] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_0b8dff0ce67f10954a49b059e348837e", "8");
  functionsIndex_[5] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_e05097c153f676e5a5ee54dcfc78256f", "4");
  functionsIndex_[6] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_e840242bf58d17d3c1d722da09ce88e0", "8");
  functionsIndex_[7] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_c9af31433001b5dfc012a552dc6d0050", "8");
  functionsIndex_[8] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_3fba4d6b818180a44cd1cae6046334dc", "12");
  functionsIndex_[9] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_aeb20dc75b9246188db857317e5e0ce7", "8");
  functionsIndex_[10] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_62689803d9871e4d9c51a648640b320b", "8");
  functionsIndex_[11] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_2fb64c9e5a67eccd413b0e913469a421", "16");
  functionsIndex_[0] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_1f1acb322ea4d0aad65172824607673c", "8");
  functionsIndex_[1] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_f3fd272e4636f6a531aabb72ee01cd5b", "16");
  functionsIndex_[2] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_12d3de0a96e9efb11136a9811bb9ed38", "4");
  functionsIndex_[14] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_f756172daf04516eec3a566adabb4335", "4");
  functionsIndex_[15] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_ddb68763ec902a97d579666a73a20118", "8");
  functionsIndex_[16] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_fba3c68b4be7558dbc65f7ce1ab57d63", "12");
  functionsIndex_[17] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_b4ca99d958f843493e58d1ef967340e1", "8");
  functionsIndex_[18] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_78d5cc76d282437b6f93ec3b82c35701", "16");
  functionsIndex_[12] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_6cf0d7268667f9b0aa4511bacf184919", "12");
  functionsIndex_[13] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_7d81cd502ee27e859735d0ea7112b5a1", "4");
  functionsIndex_[21] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_48a2a1a9d68c047e22bfba23014643d2", "4");
  functionsIndex_[22] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_852bf8296ca21c5fde5ec565cc10721d", "8");
  functionsIndex_[23] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_efd04574e0779faa83df1f2d8f9888db", "12");
  functionsIndex_[24] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_736247ff5e8036dac38163da6f666ed5", "8");
  functionsIndex_[25] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_d82d2598a7a73f4b6fcc0c09c25b08ca", "8");
  functionsIndex_[26] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_88134b978f9acb2aecdadf54aeab3c64", "16");
  functionsIndex_[27] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_152cb1b704c053d24b0dab7461ba6ea3", "8");
  functionsIndex_[28] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_eee03f337ec81d9f1783cd41e5238757", "8");
  functionsIndex_[29] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_006f08237bd7611636fc721baebfb4c5", "8");
  functionsIndex_[30] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_b794f5cd3dad7d7b575dd1fd902afdd0", "8");
  functionsIndex_[31] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_8ee2e50dd9df8f66a3c1766090dd03ab", "8");
  functionsIndex_[32] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_046aed35bbe4751691f4c34cc249a61d", "8");
  functionsIndex_[33] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_2be452e7af5bf7dfd8c5021842674497", "8");
  functionsIndex_[34] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_4dcc7a0fd025efba251ac6e9b701c2c5", "28");
  functionsIndex_[35] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_b2601a161c24ad0a1d3586246f87452c", "32");
  functionsIndex_[19] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_193599b9e345384fcdfcd47c29c55342", "12");
  functionsIndex_[20] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_7c97f17063a357d38c5fab1136ad12a0", "4");
  functionsIndex_[38] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_e65b20b7e0170b67544cd6664a4639b7", "4");
  functionsIndex_[39] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_470e981b0e41f17231ba0ae6f3033321", "8");
  functionsIndex_[40] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_04cefd138b6ea15ad909858f2a0a8f05", "12");
  functionsIndex_[41] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_aee5b1f6f0c082f2c3b0986f9f6a18c7", "8");
  functionsIndex_[42] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_93965682bace75491413e1f0b8d5a654", "16");
  functionsIndex_[36] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_b01c6003238eb46c8db5dc823d7ca678", "12");
  functionsIndex_[37] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_0147007fb99bad8cd95a139ec8795376", "4");
  functionsIndex_[45] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_236ee8b403bc99535a8a4695c0cd45cb", "8");
  functionsIndex_[46] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_2a437b7aba6bb01e81113835be8f0146", "8");
  functionsIndex_[47] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_2bcbcb850934ae0bb4c6f0cc940e6cda", "8");
  functionsIndex_[48] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_8d415c3a78a48e7e61d9fd24e7c79484", "12");
  functionsIndex_[49] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_70d2f8398bbc63b5f792b69b4ad5fecb", "12");
  functionsIndex_[50] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_1729a067d902771517388eedd7346b23", "12");
  functionsIndex_[51] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_72e2aeee66cd3abd8ab7e987321c3745", "8");
  functionsIndex_[52] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_1ea3df5a1ac1a1a687fe7325adddb6f0", "8");
  functionsIndex_[53] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_99b4f370e4f532d8b763e2cb49db92f8", "8");
  functionsIndex_[54] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_c41c742b68617f1c0590577a0a5ebc0c", "8");
  functionsIndex_[55] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_142dd2feba0fc1d262bbd0baeb441a8b", "8");
  functionsIndex_[56] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_5f5c9f81a4dff8daa6c359f1d0488fef", "12");
  functionsIndex_[57] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_9ca979fffd08fa256306d4e68d8b0e91", "8");
  functionsIndex_[58] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_6f2d77a26edc91c28d89408dbc3c271e", "8");
  functionsIndex_[59] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_c0f494b80d4ff8b232df7a75baa0700a", "4");
  functionsIndex_[60] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_d604f44bd5195e082e745e9cbc164f4c", "4");
  functionsIndex_[61] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_1710299d1c5f3b1f2b7cf3962deebbfd", "8");
  functionsIndex_[62] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_bb55aaf772ddceaadee36f4e54136bcb", "8");
  functionsIndex_[43] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_6c5ad02f91b583e29cebd0bd319ce21d", "12");
  functionsIndex_[44] = LAAW_ORTHANC_CLIENT_GET_FUNCTION(handle_, "LAAW_EXTERNC_4068241c44a9c1367fe0e57be523f207", "4");
  
  /* Check whether the functions were properly loaded */
  for (unsigned int i = 0; i <= 63; i++)
  {
    if (functionsIndex_[i] == (LAAW_ORTHANC_CLIENT_FUNCTION_TYPE) NULL)
    {
      throw ::OrthancClient::OrthancClientException("Unable to load the functions of the shared library");
    }
  }
}
}}
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
  /**
  * @brief The memory layout of the pixels (resp. voxels) of a 2D (resp. 3D) image.
  *
  * The memory layout of the pixels (resp. voxels) of a 2D (resp. 3D) image.
  *
  * @ingroup Global
  **/
  enum PixelFormat
  {
    /**
    * @brief Graylevel, signed 16bpp image.
    *
    * The image is graylevel. Each pixel is signed and stored in two bytes.
    *
    **/
    PixelFormat_SignedGrayscale16 = 5,
    /**
    * @brief Color image in RGB24 format.
    *
    * This format describes a color image. The pixels are stored in 3 consecutive bytes. The memory layout is RGB.
    *
    **/
    PixelFormat_RGB24 = 1,
    /**
    * @brief Color image in RGBA32 format.
    *
    * This format describes a color image. The pixels are stored in 4 consecutive bytes. The memory layout is RGBA.
    *
    **/
    PixelFormat_RGBA32 = 2,
    /**
    * @brief Graylevel 8bpp image.
    *
    * The image is graylevel. Each pixel is unsigned and stored in one byte.
    *
    **/
    PixelFormat_Grayscale8 = 3,
    /**
    * @brief Graylevel, unsigned 16bpp image.
    *
    * The image is graylevel. Each pixel is unsigned and stored in two bytes.
    *
    **/
    PixelFormat_Grayscale16 = 4
  };
}

namespace Orthanc
{
  /**
  * @brief The extraction mode specifies the way the values of the pixels are scaled when downloading a 2D image.
  *
  * The extraction mode specifies the way the values of the pixels are scaled when downloading a 2D image.
  *
  * @ingroup Global
  **/
  enum ImageExtractionMode
  {
    /**
    * @brief Truncation to the [-32768, 32767] range.
    *
    * Truncation to the [-32768, 32767] range.
    *
    **/
    ImageExtractionMode_Int16 = 4,
    /**
    * @brief Rescaled to 8bpp.
    *
    * The minimum value of the image is set to 0, and its maximum value is set to 255.
    *
    **/
    ImageExtractionMode_Preview = 1,
    /**
    * @brief Truncation to the [0, 255] range.
    *
    * Truncation to the [0, 255] range.
    *
    **/
    ImageExtractionMode_UInt8 = 2,
    /**
    * @brief Truncation to the [0, 65535] range.
    *
    * Truncation to the [0, 65535] range.
    *
    **/
    ImageExtractionMode_UInt16 = 3
  };
}

namespace OrthancClient
{
  /**
  * @brief Connection to an instance of %Orthanc.
  *
  * This class encapsulates a connection to a remote instance of %Orthanc through its REST API.
  *
  **/
  class OrthancConnection
  {
    friend class ::OrthancClient::Patient;
    friend class ::OrthancClient::Series;
    friend class ::OrthancClient::Study;
    friend class ::OrthancClient::Instance;
  private:
    bool isReference_;
    OrthancConnection& operator= (const OrthancConnection&); // Assignment is forbidden
    void* pimpl_;
    OrthancConnection(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
  public:
    /**
     * @brief Construct a new reference to this object.
     *
     * Construct a new reference to this object. Pay attention to the fact that when the referenced object is deleted, the content of this object will be invalid.
     *
     * @param other The original object.
     **/
    OrthancConnection(const OrthancConnection& other) : isReference_(true), pimpl_(other.pimpl_) { }
    inline OrthancConnection(const ::std::string& orthancUrl);
    inline OrthancConnection(const ::std::string& orthancUrl, const ::std::string& username, const ::std::string& password);
    inline ~OrthancConnection();
    inline LAAW_UINT32 GetThreadCount() const;
    inline void SetThreadCount(LAAW_UINT32 threadCount);
    inline void Reload();
    inline ::std::string GetOrthancUrl() const;
    inline LAAW_UINT32 GetPatientCount();
    inline ::OrthancClient::Patient GetPatient(LAAW_UINT32 index);
    inline void DeletePatient(LAAW_UINT32 index);
    inline void StoreFile(const ::std::string& filename);
    inline void Store(const void* dicom, LAAW_UINT64 size);
  };
}

namespace OrthancClient
{
  /**
  * @brief Connection to a patient stored in %Orthanc.
  *
  * This class encapsulates a connection to a patient from a remote instance of %Orthanc.
  *
  **/
  class Patient
  {
    friend class ::OrthancClient::OrthancConnection;
    friend class ::OrthancClient::Series;
    friend class ::OrthancClient::Study;
    friend class ::OrthancClient::Instance;
  private:
    bool isReference_;
    Patient& operator= (const Patient&); // Assignment is forbidden
    void* pimpl_;
    Patient(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
  public:
    /**
     * @brief Construct a new reference to this object.
     *
     * Construct a new reference to this object. Pay attention to the fact that when the referenced object is deleted, the content of this object will be invalid.
     *
     * @param other The original object.
     **/
    Patient(const Patient& other) : isReference_(true), pimpl_(other.pimpl_) { }
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
  /**
  * @brief Connection to a series stored in %Orthanc.
  *
  * This class encapsulates a connection to a series from a remote instance of %Orthanc.
  *
  **/
  class Series
  {
    friend class ::OrthancClient::OrthancConnection;
    friend class ::OrthancClient::Patient;
    friend class ::OrthancClient::Study;
    friend class ::OrthancClient::Instance;
  private:
    bool isReference_;
    Series& operator= (const Series&); // Assignment is forbidden
    void* pimpl_;
    Series(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
  public:
    /**
     * @brief Construct a new reference to this object.
     *
     * Construct a new reference to this object. Pay attention to the fact that when the referenced object is deleted, the content of this object will be invalid.
     *
     * @param other The original object.
     **/
    Series(const Series& other) : isReference_(true), pimpl_(other.pimpl_) { }
    inline Series(::OrthancClient::OrthancConnection& connection, const ::std::string& id);
    inline ~Series();
    inline void Reload();
    inline LAAW_UINT32 GetInstanceCount();
    inline ::OrthancClient::Instance GetInstance(LAAW_UINT32 index);
    inline ::std::string GetId() const;
    inline ::std::string GetUrl() const;
    inline ::std::string GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const;
    inline bool Is3DImage();
    inline LAAW_UINT32 GetWidth();
    inline LAAW_UINT32 GetHeight();
    inline float GetVoxelSizeX();
    inline float GetVoxelSizeY();
    inline float GetVoxelSizeZ();
    inline float GetSliceThickness();
    inline void Load3DImage(void* target, ::Orthanc::PixelFormat format, LAAW_INT64 lineStride, LAAW_INT64 stackStride);
    inline void Load3DImage(void* target, ::Orthanc::PixelFormat format, LAAW_INT64 lineStride, LAAW_INT64 stackStride, float progress[]);
  };
}

namespace OrthancClient
{
  /**
  * @brief Connection to a study stored in %Orthanc.
  *
  * This class encapsulates a connection to a study from a remote instance of %Orthanc.
  *
  **/
  class Study
  {
    friend class ::OrthancClient::OrthancConnection;
    friend class ::OrthancClient::Patient;
    friend class ::OrthancClient::Series;
    friend class ::OrthancClient::Instance;
  private:
    bool isReference_;
    Study& operator= (const Study&); // Assignment is forbidden
    void* pimpl_;
    Study(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
  public:
    /**
     * @brief Construct a new reference to this object.
     *
     * Construct a new reference to this object. Pay attention to the fact that when the referenced object is deleted, the content of this object will be invalid.
     *
     * @param other The original object.
     **/
    Study(const Study& other) : isReference_(true), pimpl_(other.pimpl_) { }
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
  /**
  * @brief Connection to an instance stored in %Orthanc.
  *
  * This class encapsulates a connection to an image instance from a remote instance of %Orthanc.
  *
  **/
  class Instance
  {
    friend class ::OrthancClient::OrthancConnection;
    friend class ::OrthancClient::Patient;
    friend class ::OrthancClient::Series;
    friend class ::OrthancClient::Study;
  private:
    bool isReference_;
    Instance& operator= (const Instance&); // Assignment is forbidden
    void* pimpl_;
    Instance(void* pimpl) : isReference_(true), pimpl_(pimpl) {}
  public:
    /**
     * @brief Construct a new reference to this object.
     *
     * Construct a new reference to this object. Pay attention to the fact that when the referenced object is deleted, the content of this object will be invalid.
     *
     * @param other The original object.
     **/
    Instance(const Instance& other) : isReference_(true), pimpl_(other.pimpl_) { }
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
    inline LAAW_UINT64 GetDicomSize();
    inline const void* GetDicom();
    inline void DiscardImage();
    inline void DiscardDicom();
    inline void LoadTagContent(const ::std::string& path);
    inline ::std::string GetLoadedTagContent() const;
  };
}

namespace OrthancClient
{
  /**
  * @brief Create a connection to an instance of %Orthanc.
  *
  * Create a connection to an instance of %Orthanc.
  *
  * @param orthancUrl URL to which the REST API of %Orthanc is listening.
  **/
  inline OrthancConnection::OrthancConnection(const ::std::string& orthancUrl)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(0);
    char* error = function(&pimpl_, orthancUrl.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Create a connection to an instance of %Orthanc, with authentication.
  *
  * Create a connection to an instance of %Orthanc, with authentication.
  *
  * @param orthancUrl URL to which the REST API of %Orthanc is listening.
  * @param username The username.
  * @param password The password.
  **/
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
    error = error;  // Remove warning about unused variable
  }
  /**
  * @brief Returns the number of threads for this connection.
  *
  * Returns the number of simultaneous connections that are used when downloading information from this instance of %Orthanc.
  *
  * @return The number of threads.
  **/
  inline LAAW_UINT32 OrthancConnection::GetThreadCount() const
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(3);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Sets the number of threads for this connection.
  *
  * Sets the number of simultaneous connections that are used when downloading information from this instance of %Orthanc.
  *
  * @param threadCount The number of threads.
  **/
  inline void OrthancConnection::SetThreadCount(LAAW_UINT32 threadCount)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(4);
    char* error = function(pimpl_, threadCount);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Reload the list of the patients.
  *
  * This method will reload the list of the patients from the remote instance of %Orthanc. Pay attention to the fact that the patients that have been previously returned by GetPatient() will be invalidated.
  *
  **/
  inline void OrthancConnection::Reload()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(5);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Returns the URL of this instance of %Orthanc.
  *
  * Returns the URL of the remote %Orthanc instance to which this object is connected.
  *
  * @return The URL.
  **/
  inline ::std::string OrthancConnection::GetOrthancUrl() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(6);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  /**
  * @brief Returns the number of patients.
  *
  * Returns the number of patients that are stored in the remote instance of %Orthanc.
  *
  * @return The number of patients.
  **/
  inline LAAW_UINT32 OrthancConnection::GetPatientCount()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(7);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get some patient.
  *
  * This method will return an object that contains information about some patient. The patients are indexed by a number between 0 (inclusive) and the result of GetPatientCount() (exclusive).
  *
  * @param index The index of the patient of interest.
  * @return The patient.
  **/
  inline ::OrthancClient::Patient OrthancConnection::GetPatient(LAAW_UINT32 index)
  {
    void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(8);
    char* error = function(pimpl_, &result_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return ::OrthancClient::Patient(result_);
  }
  /**
  * @brief Delete some patient.
  *
  * Delete some patient from the remote instance of %Orthanc. Pay attention to the fact that the patients that have been previously returned by GetPatient() will be invalidated.
  *
  * @param index The index of the patient of interest.
  * @return The patient.
  **/
  inline void OrthancConnection::DeletePatient(LAAW_UINT32 index)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(9);
    char* error = function(pimpl_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Send a DICOM file.
  *
  * This method will store a DICOM file in the remote instance of %Orthanc. Pay attention to the fact that the patients that have been previously returned by GetPatient() will be invalidated.
  *
  * @param filename Path to the DICOM file
  **/
  inline void OrthancConnection::StoreFile(const ::std::string& filename)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(10);
    char* error = function(pimpl_, filename.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Send a DICOM file that is contained inside a memory buffer.
  *
  * This method will store a DICOM file in the remote instance of %Orthanc. Pay attention to the fact that the patients that have been previously returned by GetPatient() will be invalidated.
  *
  * @param dicom The memory buffer containing the DICOM file.
  * @param size The size of the DICOM file.
  **/
  inline void OrthancConnection::Store(const void* dicom, LAAW_UINT64 size)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, const void*, LAAW_UINT64);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(11);
    char* error = function(pimpl_, dicom, size);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
}

namespace OrthancClient
{
  /**
  * @brief Create a connection to some patient.
  *
  * Create a connection to some patient.
  *
  * @param connection The remote instance of %Orthanc.
  * @param id The %Orthanc identifier of the patient.
  **/
  inline Patient::Patient(::OrthancClient::OrthancConnection& connection, const ::std::string& id)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(12);
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
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(13);
    char* error = function(pimpl_);
    error = error;  // Remove warning about unused variable
  }
  /**
  * @brief Reload the studies of this patient.
  *
  * This method will reload the list of the studies of this patient. Pay attention to the fact that the studies that have been previously returned by GetStudy() will be invalidated.
  *
  **/
  inline void Patient::Reload()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(14);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Return the number of studies for this patient.
  *
  * Return the number of studies for this patient.
  *
  * @return The number of studies.
  **/
  inline LAAW_UINT32 Patient::GetStudyCount()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(15);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get some study of this patient.
  *
  * This method will return an object that contains information about some study. The studies are indexed by a number between 0 (inclusive) and the result of GetStudyCount() (exclusive).
  *
  * @param index The index of the study of interest.
  * @return The study.
  **/
  inline ::OrthancClient::Study Patient::GetStudy(LAAW_UINT32 index)
  {
    void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(16);
    char* error = function(pimpl_, &result_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return ::OrthancClient::Study(result_);
  }
  /**
  * @brief Get the %Orthanc identifier of this patient.
  *
  * Get the %Orthanc identifier of this patient.
  *
  * @return The identifier.
  **/
  inline ::std::string Patient::GetId() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(17);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  /**
  * @brief Get the value of one of the main DICOM tags for this patient.
  *
  * Get the value of one of the main DICOM tags for this patient.
  *
  * @param tag The name of the tag of interest ("PatientName", "PatientID", "PatientSex" or "PatientBirthDate").
  * @param defaultValue The default value to be returned if this tag does not exist.
  * @return The value of the tag.
  **/
  inline ::std::string Patient::GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**, const char*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(18);
    char* error = function(pimpl_, &result_, tag.c_str(), defaultValue.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
}

namespace OrthancClient
{
  /**
  * @brief Create a connection to some series.
  *
  * Create a connection to some series.
  *
  * @param connection The remote instance of %Orthanc.
  * @param id The %Orthanc identifier of the series.
  **/
  inline Series::Series(::OrthancClient::OrthancConnection& connection, const ::std::string& id)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(19);
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
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(20);
    char* error = function(pimpl_);
    error = error;  // Remove warning about unused variable
  }
  /**
  * @brief Reload the instances of this series.
  *
  * This method will reload the list of the instances of this series. Pay attention to the fact that the instances that have been previously returned by GetInstance() will be invalidated.
  *
  **/
  inline void Series::Reload()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(21);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Return the number of instances for this series.
  *
  * Return the number of instances for this series.
  *
  * @return The number of instances.
  **/
  inline LAAW_UINT32 Series::GetInstanceCount()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(22);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get some instance of this series.
  *
  * This method will return an object that contains information about some instance. The instances are indexed by a number between 0 (inclusive) and the result of GetInstanceCount() (exclusive).
  *
  * @param index The index of the instance of interest.
  * @return The instance.
  **/
  inline ::OrthancClient::Instance Series::GetInstance(LAAW_UINT32 index)
  {
    void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(23);
    char* error = function(pimpl_, &result_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return ::OrthancClient::Instance(result_);
  }
  /**
  * @brief Get the %Orthanc identifier of this series.
  *
  * Get the %Orthanc identifier of this series.
  *
  * @return The identifier.
  **/
  inline ::std::string Series::GetId() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(24);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  /**
  * @brief Returns the URL to this series.
  *
  * Returns the URL to this series.
  *
  * @return The URL.
  **/
  inline ::std::string Series::GetUrl() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(25);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  /**
  * @brief Get the value of one of the main DICOM tags for this series.
  *
  * Get the value of one of the main DICOM tags for this series.
  *
  * @param tag The name of the tag of interest ("Modality", "Manufacturer", "SeriesDate", "SeriesDescription", "SeriesInstanceUID"...).
  * @param defaultValue The default value to be returned if this tag does not exist.
  * @return The value of the tag.
  **/
  inline ::std::string Series::GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**, const char*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(26);
    char* error = function(pimpl_, &result_, tag.c_str(), defaultValue.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  /**
  * @brief Test whether this series encodes a 3D image that can be downloaded from %Orthanc.
  *
  * Test whether this series encodes a 3D image that can be downloaded from %Orthanc.
  *
  * @return "true" if and only if this is a 3D image.
  **/
  inline bool Series::Is3DImage()
  {
    LAAW_INT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_INT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(27);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_ != 0;
  }
  /**
  * @brief Get the width of the 3D image.
  *
  * Get the width of the 3D image (i.e. along the X-axis). This call is only valid if this series corresponds to a 3D image.
  *
  * @return The width.
  **/
  inline LAAW_UINT32 Series::GetWidth()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(28);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the height of the 3D image.
  *
  * Get the height of the 3D image (i.e. along the Y-axis). This call is only valid if this series corresponds to a 3D image.
  *
  * @return The height.
  **/
  inline LAAW_UINT32 Series::GetHeight()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(29);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the physical size of a voxel along the X-axis.
  *
  * Get the physical size of a voxel along the X-axis. This call is only valid if this series corresponds to a 3D image.
  *
  * @return The voxel size.
  **/
  inline float Series::GetVoxelSizeX()
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, float*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(30);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the physical size of a voxel along the Y-axis.
  *
  * Get the physical size of a voxel along the Y-axis. This call is only valid if this series corresponds to a 3D image.
  *
  * @return The voxel size.
  **/
  inline float Series::GetVoxelSizeY()
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, float*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(31);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the physical size of a voxel along the Z-axis.
  *
  * Get the physical size of a voxel along the Z-axis. This call is only valid if this series corresponds to a 3D image.
  *
  * @return The voxel size.
  **/
  inline float Series::GetVoxelSizeZ()
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, float*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(32);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the slice thickness.
  *
  * Get the slice thickness. This call is only valid if this series corresponds to a 3D image.
  *
  * @return The slice thickness.
  **/
  inline float Series::GetSliceThickness()
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, float*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(33);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Load the 3D image into a memory buffer.
  *
  * Load the 3D image into a memory buffer. This call is only valid if this series corresponds to a 3D image. The "target" buffer must be wide enough to store all the voxels of the image.
  *
  * @param target The target memory buffer.
  * @param format The memory layout of the voxels.
  * @param lineStride The number of bytes between two lines in the target memory buffer.
  * @param stackStride The number of bytes between two 2D slices in the target memory buffer.
  **/
  inline void Series::Load3DImage(void* target, ::Orthanc::PixelFormat format, LAAW_INT64 lineStride, LAAW_INT64 stackStride)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void*, LAAW_INT32, LAAW_INT64, LAAW_INT64);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(34);
    char* error = function(pimpl_, target, format, lineStride, stackStride);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Load the 3D image into a memory buffer.
  *
  * Load the 3D image into a memory buffer. This call is only valid if this series corresponds to a 3D image. The "target" buffer must be wide enough to store all the voxels of the image. This method will also update a progress indicator to monitor the loading of the image.
  *
  * @param target The target memory buffer.
  * @param format The memory layout of the voxels.
  * @param lineStride The number of bytes between two lines in the target memory buffer.
  * @param stackStride The number of bytes between two 2D slices in the target memory buffer.
  * @param progress A pointer to a floating-point number that is continuously updated by the download threads to reflect the percentage of completion (between 0 and 1). This value can be read from a separate thread.
  **/
  inline void Series::Load3DImage(void* target, ::Orthanc::PixelFormat format, LAAW_INT64 lineStride, LAAW_INT64 stackStride, float progress[])
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void*, LAAW_INT32, LAAW_INT64, LAAW_INT64, float*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(35);
    char* error = function(pimpl_, target, format, lineStride, stackStride, progress);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
}

namespace OrthancClient
{
  /**
  * @brief Create a connection to some study.
  *
  * Create a connection to some study.
  *
  * @param connection The remote instance of %Orthanc.
  * @param id The %Orthanc identifier of the study.
  **/
  inline Study::Study(::OrthancClient::OrthancConnection& connection, const ::std::string& id)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(36);
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
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(37);
    char* error = function(pimpl_);
    error = error;  // Remove warning about unused variable
  }
  /**
  * @brief Reload the series of this study.
  *
  * This method will reload the list of the series of this study. Pay attention to the fact that the series that have been previously returned by GetSeries() will be invalidated.
  *
  **/
  inline void Study::Reload()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(38);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Return the number of series for this study.
  *
  * Return the number of series for this study.
  *
  * @return The number of series.
  **/
  inline LAAW_UINT32 Study::GetSeriesCount()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(39);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get some series of this study.
  *
  * This method will return an object that contains information about some series. The series are indexed by a number between 0 (inclusive) and the result of GetSeriesCount() (exclusive).
  *
  * @param index The index of the series of interest.
  * @return The series.
  **/
  inline ::OrthancClient::Series Study::GetSeries(LAAW_UINT32 index)
  {
    void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(40);
    char* error = function(pimpl_, &result_, index);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return ::OrthancClient::Series(result_);
  }
  /**
  * @brief Get the %Orthanc identifier of this study.
  *
  * Get the %Orthanc identifier of this study.
  *
  * @return The identifier.
  **/
  inline ::std::string Study::GetId() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(41);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  /**
  * @brief Get the value of one of the main DICOM tags for this study.
  *
  * Get the value of one of the main DICOM tags for this study.
  *
  * @param tag The name of the tag of interest ("StudyDate", "StudyDescription", "StudyInstanceUID" or "StudyTime").
  * @param defaultValue The default value to be returned if this tag does not exist.
  * @return The value of the tag.
  **/
  inline ::std::string Study::GetMainDicomTag(const ::std::string& tag, const ::std::string& defaultValue) const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**, const char*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(42);
    char* error = function(pimpl_, &result_, tag.c_str(), defaultValue.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
}

namespace OrthancClient
{
  /**
  * @brief Create a connection to some image instance.
  *
  * Create a connection to some image instance.
  *
  * @param connection The remote instance of %Orthanc.
  * @param id The %Orthanc identifier of the image instance.
  **/
  inline Instance::Instance(::OrthancClient::OrthancConnection& connection, const ::std::string& id)
  {
    isReference_ = false;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void**, void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(43);
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
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(44);
    char* error = function(pimpl_);
    error = error;  // Remove warning about unused variable
  }
  /**
  * @brief Get the %Orthanc identifier of this identifier.
  *
  * Get the %Orthanc identifier of this identifier.
  *
  * @return The identifier.
  **/
  inline ::std::string Instance::GetId() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(45);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  /**
  * @brief Set the extraction mode for the 2D image corresponding to this instance.
  *
  * Set the extraction mode for the 2D image corresponding to this instance.
  *
  * @param mode The extraction mode.
  **/
  inline void Instance::SetImageExtractionMode(::Orthanc::ImageExtractionMode mode)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_INT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(46);
    char* error = function(pimpl_, mode);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Get the extraction mode for the 2D image corresponding to this instance.
  *
  * Get the extraction mode for the 2D image corresponding to this instance.
  *
  * @return The extraction mode.
  **/
  inline ::Orthanc::ImageExtractionMode Instance::GetImageExtractionMode() const
  {
    LAAW_INT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, LAAW_INT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(47);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return static_cast< ::Orthanc::ImageExtractionMode >(result_);
  }
  /**
  * @brief Get the string value of some DICOM tag of this instance.
  *
  * Get the string value of some DICOM tag of this instance.
  *
  * @param tag The name of the tag of interest.
  * @return The value of the tag.
  **/
  inline ::std::string Instance::GetTagAsString(const ::std::string& tag) const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(48);
    char* error = function(pimpl_, &result_, tag.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
  /**
  * @brief Get the floating point value that is stored in some DICOM tag of this instance.
  *
  * Get the floating point value that is stored in some DICOM tag of this instance.
  *
  * @param tag The name of the tag of interest.
  * @return The value of the tag.
  **/
  inline float Instance::GetTagAsFloat(const ::std::string& tag) const
  {
    float result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, float*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(49);
    char* error = function(pimpl_, &result_, tag.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the integer value that is stored in some DICOM tag of this instance.
  *
  * Get the integer value that is stored in some DICOM tag of this instance.
  *
  * @param tag The name of the tag of interest.
  * @return The value of the tag.
  **/
  inline LAAW_INT32 Instance::GetTagAsInt(const ::std::string& tag) const
  {
    LAAW_INT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, LAAW_INT32*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(50);
    char* error = function(pimpl_, &result_, tag.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the width of the 2D image.
  *
  * Get the width of the 2D image that is encoded by this DICOM instance.
  *
  * @return The width.
  **/
  inline LAAW_UINT32 Instance::GetWidth()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(51);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the height of the 2D image.
  *
  * Get the height of the 2D image that is encoded by this DICOM instance.
  *
  * @return The height.
  **/
  inline LAAW_UINT32 Instance::GetHeight()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(52);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the number of bytes between two lines of the image (pitch).
  *
  * Get the number of bytes between two lines of the image in the memory buffer returned by GetBuffer(). This value depends on the extraction mode for the image.
  *
  * @return The pitch.
  **/
  inline LAAW_UINT32 Instance::GetPitch()
  {
    LAAW_UINT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(53);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get the format of the pixels of the 2D image.
  *
  * Return the memory layout that is used for the 2D image that is encoded by this DICOM instance. This value depends on the extraction mode for the image.
  *
  * @return The pixel format.
  **/
  inline ::Orthanc::PixelFormat Instance::GetPixelFormat()
  {
    LAAW_INT32 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_INT32*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(54);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return static_cast< ::Orthanc::PixelFormat >(result_);
  }
  /**
  * @brief Access the memory buffer in which the raw pixels of the 2D image are stored.
  *
  * Access the memory buffer in which the raw pixels of the 2D image are stored.
  *
  * @return A pointer to the memory buffer.
  **/
  inline const void* Instance::GetBuffer()
  {
    const void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, const void**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(55);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return reinterpret_cast< const void* >(result_);
  }
  /**
  * @brief Access the memory buffer in which the raw pixels of some line of the 2D image are stored.
  *
  * Access the memory buffer in which the raw pixels of some line of the 2D image are stored.
  *
  * @param y The line of interest.
  * @return A pointer to the memory buffer.
  **/
  inline const void* Instance::GetBuffer(LAAW_UINT32 y)
  {
    const void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, const void**, LAAW_UINT32);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(56);
    char* error = function(pimpl_, &result_, y);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return reinterpret_cast< const void* >(result_);
  }
  /**
  * @brief Get the size of the DICOM file corresponding to this instance.
  *
  * Get the size of the DICOM file corresponding to this instance.
  *
  * @return The file size.
  **/
  inline LAAW_UINT64 Instance::GetDicomSize()
  {
    LAAW_UINT64 result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, LAAW_UINT64*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(57);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return result_;
  }
  /**
  * @brief Get a pointer to the content of the DICOM file corresponding to this instance.
  *
  * Get a pointer to the content of the DICOM file corresponding to this instance.
  *
  * @return The DICOM file.
  **/
  inline const void* Instance::GetDicom()
  {
    const void* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, const void**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(58);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return reinterpret_cast< const void* >(result_);
  }
  /**
  * @brief Discard the downloaded 2D image, so as to make room in memory.
  *
  * Discard the downloaded 2D image, so as to make room in memory.
  *
  **/
  inline void Instance::DiscardImage()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(59);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Discard the downloaded DICOM file, so as to make room in memory.
  *
  * Discard the downloaded DICOM file, so as to make room in memory.
  *
  **/
  inline void Instance::DiscardDicom()
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(60);
    char* error = function(pimpl_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Load a raw tag from the DICOM file.
  *
  * Load a raw tag from the DICOM file.
  *
  * @param path The path to the tag of interest (e.g. "0020-000d").
  **/
  inline void Instance::LoadTagContent(const ::std::string& path)
  {
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (void*, const char*);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(61);
    char* error = function(pimpl_, path.c_str());
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
  }
  /**
  * @brief Return the value of the raw tag that was loaded by LoadContent.
  *
  * Return the value of the raw tag that was loaded by LoadContent.
  *
  * @return The tag value.
  **/
  inline ::std::string Instance::GetLoadedTagContent() const
  {
    const char* result_;
    typedef char* (LAAW_ORTHANC_CLIENT_CALL_CONV* Function) (const void*, const char**);
    Function function = (Function) ::OrthancClient::Internals::Library::GetInstance().GetFunction(62);
    char* error = function(pimpl_, &result_);
    ::OrthancClient::Internals::Library::GetInstance().ThrowExceptionIfNeeded(error);
    return std::string(result_);
  }
}

