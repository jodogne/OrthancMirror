# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program. If not, see
# <http://www.gnu.org/licenses/>.


if (BOOST_STATIC)
  ##
  ## Parameters for static compilation of Boost 
  ##
  
  set(BOOST_NAME boost_1_69_0)
  set(BOOST_VERSION 1.69.0)
  set(BOOST_BCP_SUFFIX bcpdigest-1.5.6)
  set(BOOST_MD5 "579bccc0ea4d1a261c1d0c5e27446c3d")
  set(BOOST_URL "https://orthanc.uclouvain.be/third-party-downloads/${BOOST_NAME}_${BOOST_BCP_SUFFIX}.tar.gz")
  set(BOOST_SOURCES_DIR ${CMAKE_BINARY_DIR}/${BOOST_NAME})

  if (IS_DIRECTORY "${BOOST_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()

  DownloadPackage(${BOOST_MD5} ${BOOST_URL} "${BOOST_SOURCES_DIR}")


  ##
  ## Patching boost
  ## 

  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/boost-${BOOST_VERSION}-linux-standard-base.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (FirstRun AND Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()


  ##
  ## Generic configuration of Boost
  ## 

  if (CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-isystem ${BOOST_SOURCES_DIR})
  endif()

  include_directories(
    BEFORE ${BOOST_SOURCES_DIR}
    )

  if (ORTHANC_BUILDING_FRAMEWORK_LIBRARY)
    add_definitions(
      # Packaging Boost inside the Orthanc Framework DLL
      -DBOOST_ALL_DYN_LINK      # Expose Boost symbols into the DLL
      -DBOOST_THREAD_BUILD_DLL
      -DBOOST_REGEX_BUILD_DLL
      -DBOOST_IOSTREAMS_SOURCE
      )
  else()
    add_definitions(
      # Static build of Boost (this was the only possibility in
      # Orthanc <= 1.7.1)
      -DBOOST_ALL_NO_LIB 
      -DBOOST_ALL_NOLIB 
      -DBOOST_DATE_TIME_NO_LIB 
      -DBOOST_THREAD_BUILD_LIB
      -DBOOST_PROGRAM_OPTIONS_NO_LIB
      -DBOOST_REGEX_NO_LIB
      -DBOOST_SYSTEM_NO_LIB
      -DBOOST_LOCALE_NO_LIB
      )
  endif()

  add_definitions(
    # In static builds, explicitly prevent Boost from using the system
    # locale in lexical casts. This is notably important if
    # "boost::lexical_cast<double>()" is applied to strings containing
    # "," instead of "." as decimal separators. Check out function
    # "OrthancStone::LinearAlgebra::ParseVector()".
    -DBOOST_LEXICAL_CAST_ASSUME_C_LOCALE
    )

  set(BOOST_SOURCES
    ${BOOST_SOURCES_DIR}/libs/system/src/error_code.cpp
    )

  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase" OR
      "${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    add_definitions(
      -DBOOST_SYSTEM_USE_STRERROR=1
      )
  endif()

  
  ##
  ## Configuration of boost::thread
  ##
  
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
      CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
      CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR
      CMAKE_SYSTEM_NAME STREQUAL "kFreeBSD" OR
      CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" OR
      CMAKE_SYSTEM_NAME STREQUAL "PNaCl" OR
      CMAKE_SYSTEM_NAME STREQUAL "NaCl32" OR
      CMAKE_SYSTEM_NAME STREQUAL "NaCl64" OR
      CMAKE_SYSTEM_NAME STREQUAL "Android")
    list(APPEND BOOST_SOURCES
      ${BOOST_SOURCES_DIR}/libs/atomic/src/lockpool.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/pthread/once.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/pthread/thread.cpp
      )

    if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase" OR
        CMAKE_SYSTEM_NAME STREQUAL "PNaCl" OR
        CMAKE_SYSTEM_NAME STREQUAL "NaCl32" OR
        CMAKE_SYSTEM_NAME STREQUAL "NaCl64")
      add_definitions(-DBOOST_HAS_SCHED_YIELD=1)
    endif()

    # Fix for error: "boost_1_69_0/boost/chrono/detail/inlined/mac/thread_clock.hpp:54:28: 
    # error: use of undeclared identifier 'pthread_mach_thread_np'"
    # https://github.com/envoyproxy/envoy/pull/1785
    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      add_definitions(-D_DARWIN_C_SOURCE=1)
    endif()

  elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND BOOST_SOURCES
      ${BOOST_SOURCES_DIR}/libs/thread/src/win32/tss_dll.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/win32/thread.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/win32/tss_pe.cpp
      )

  elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    # No support for threads in asm.js/WebAssembly

  else()
    message(FATAL_ERROR "Support your platform here")
  endif()


  ##
  ## Configuration of boost::regex
  ##
  
  aux_source_directory(${BOOST_SOURCES_DIR}/libs/regex/src BOOST_REGEX_SOURCES)

  list(APPEND BOOST_SOURCES
    ${BOOST_REGEX_SOURCES}
    )


  ##
  ## Configuration of boost::datetime
  ##
  
  list(APPEND BOOST_SOURCES
    ${BOOST_SOURCES_DIR}/libs/date_time/src/gregorian/greg_month.cpp
    )


  ##
  ## Configuration of boost::filesystem and boost::iostreams
  ## 

  if (CMAKE_SYSTEM_NAME STREQUAL "PNaCl" OR
      CMAKE_SYSTEM_NAME STREQUAL "NaCl32" OR
      CMAKE_SYSTEM_NAME STREQUAL "NaCl64" OR
      CMAKE_SYSTEM_NAME STREQUAL "Android")
    # boost::filesystem is not available on PNaCl
    add_definitions(
      -DBOOST_HAS_FILESYSTEM_V3=0
      -D__INTEGRITY=1
      )
  else()
    add_definitions(
      -DBOOST_HAS_FILESYSTEM_V3=1
      )
    list(APPEND BOOST_SOURCES
      ${BOOST_NAME}/libs/filesystem/src/codecvt_error_category.cpp
      ${BOOST_NAME}/libs/filesystem/src/operations.cpp
      ${BOOST_NAME}/libs/filesystem/src/path.cpp
      ${BOOST_NAME}/libs/filesystem/src/path_traits.cpp
      )

    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
        CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" OR
        CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
     list(APPEND BOOST_SOURCES
        ${BOOST_SOURCES_DIR}/libs/filesystem/src/utf8_codecvt_facet.cpp
        )

    elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
      list(APPEND BOOST_SOURCES
        ${BOOST_NAME}/libs/filesystem/src/windows_file_codecvt.cpp
        )
    endif()
  endif()

  list(APPEND BOOST_SOURCES
    ${BOOST_NAME}/libs/iostreams/src/file_descriptor.cpp
    )
  

  ##
  ## Configuration of boost::locale
  ## 

  if (NOT ENABLE_LOCALE)
    message("boost::locale is disabled")
  else()
    set(BOOST_ICU_SOURCES
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/boundary.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/codecvt.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/collator.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/conversion.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/date_time.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/formatter.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/icu_backend.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/numeric.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/icu/time_zone.cpp
      )

    list(APPEND BOOST_SOURCES
      ${BOOST_SOURCES_DIR}/libs/locale/src/encoding/codepage.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/shared/generator.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/shared/date_time.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/shared/formatting.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/shared/ids.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/shared/localization_backend.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/shared/message.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/shared/mo_lambda.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/util/codecvt_converter.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/util/default_locale.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/util/gregorian.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/util/info.cpp
      ${BOOST_SOURCES_DIR}/libs/locale/src/util/locale_data.cpp
      )        

    if (CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" OR
        CMAKE_SYSTEM_VERSION STREQUAL "LinuxStandardBase")
      add_definitions(
        -DBOOST_LOCALE_NO_WINAPI_BACKEND=1
        -DBOOST_LOCALE_NO_POSIX_BACKEND=1
        )
      
      list(APPEND BOOST_SOURCES
        ${BOOST_SOURCES_DIR}/libs/locale/src/std/codecvt.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/std/collate.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/std/converter.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/std/numeric.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/std/std_backend.cpp
        )

      if (BOOST_LOCALE_BACKEND STREQUAL "gcc" OR
          BOOST_LOCALE_BACKEND STREQUAL "libiconv")
        add_definitions(-DBOOST_LOCALE_WITH_ICONV=1)
      elseif (BOOST_LOCALE_BACKEND STREQUAL "icu")
        add_definitions(-DBOOST_LOCALE_WITH_ICU=1)
        list(APPEND BOOST_SOURCES ${BOOST_ICU_SOURCES})
      else()
        message(FATAL_ERROR "Unsupported value for BOOST_LOCALE_BACKEND: ${BOOST_LOCALE_BACKEND}")
      endif()

    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
            CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
            CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR
            CMAKE_SYSTEM_NAME STREQUAL "kFreeBSD" OR
            CMAKE_SYSTEM_NAME STREQUAL "PNaCl" OR
            CMAKE_SYSTEM_NAME STREQUAL "NaCl32" OR
            CMAKE_SYSTEM_NAME STREQUAL "NaCl64" OR
            CMAKE_SYSTEM_NAME STREQUAL "Emscripten") # For WebAssembly or asm.js
      add_definitions(
        -DBOOST_LOCALE_NO_WINAPI_BACKEND=1
        -DBOOST_LOCALE_NO_STD_BACKEND=1
        )
      
      list(APPEND BOOST_SOURCES
        ${BOOST_SOURCES_DIR}/libs/locale/src/posix/codecvt.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/posix/collate.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/posix/converter.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/posix/numeric.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/posix/posix_backend.cpp
        )

      if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten" OR
          BOOST_LOCALE_BACKEND STREQUAL "gcc" OR
          BOOST_LOCALE_BACKEND STREQUAL "libiconv")
        # In WebAssembly or asm.js, we rely on the version of iconv
        # that is shipped with the stdlib
        add_definitions(-DBOOST_LOCALE_WITH_ICONV=1)
      elseif (BOOST_LOCALE_BACKEND STREQUAL "icu")
        add_definitions(-DBOOST_LOCALE_WITH_ICU=1)
        list(APPEND BOOST_SOURCES ${BOOST_ICU_SOURCES})
      else()
        message(FATAL_ERROR "Unsupported value for BOOST_LOCALE_BACKEND: ${BOOST_LOCALE_BACKEND}")
      endif()

    elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
      add_definitions(
        -DBOOST_LOCALE_NO_POSIX_BACKEND=1
        -DBOOST_LOCALE_NO_STD_BACKEND=1
        )

      list(APPEND BOOST_SOURCES
        ${BOOST_SOURCES_DIR}/libs/locale/src/win32/collate.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/win32/converter.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/win32/lcid.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/win32/numeric.cpp
        ${BOOST_SOURCES_DIR}/libs/locale/src/win32/win_backend.cpp
        )

      # Starting with release 0.8.2, Orthanc statically links against
      # libiconv on Windows. Indeed, the "WCONV" library of Windows XP
      # seems not to support properly several codepages (notably
      # "Latin3", "Hebrew", and "Arabic"). Set "BOOST_LOCALE_BACKEND"
      # to "wconv" to use WCONV anyway.

      if (BOOST_LOCALE_BACKEND STREQUAL "libiconv")
        add_definitions(-DBOOST_LOCALE_WITH_ICONV=1)
      elseif (BOOST_LOCALE_BACKEND STREQUAL "icu")
        add_definitions(-DBOOST_LOCALE_WITH_ICU=1)
        list(APPEND BOOST_SOURCES ${BOOST_ICU_SOURCES})
      elseif (BOOST_LOCALE_BACKEND STREQUAL "wconv")
        message("Using Window's wconv")
        add_definitions(-DBOOST_LOCALE_WITH_WCONV=1)
      else()
        message(FATAL_ERROR "Unsupported value for BOOST_LOCALE_BACKEND on Windows: ${BOOST_LOCALE_BACKEND}")
      endif()

    else()
      message(FATAL_ERROR "Support your platform here")
    endif()
  endif()

  
  source_group(ThirdParty\\boost REGULAR_EXPRESSION ${BOOST_SOURCES_DIR}/.*)

endif()
