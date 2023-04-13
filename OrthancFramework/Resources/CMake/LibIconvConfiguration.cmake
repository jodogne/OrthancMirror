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


message("Using libiconv")

if (STATIC_BUILD OR NOT USE_SYSTEM_LIBICONV)
  set(LIBICONV_SOURCES_DIR ${CMAKE_BINARY_DIR}/libiconv-1.15)
  set(LIBICONV_URL "https://orthanc.uclouvain.be/third-party-downloads/libiconv-1.15.tar.gz")
  set(LIBICONV_MD5 "ace8b5f2db42f7b3b3057585e80d9808")

  DownloadPackage(${LIBICONV_MD5} ${LIBICONV_URL} "${LIBICONV_SOURCES_DIR}")

  # Disable the support of libiconv that is shipped by default with
  # the C standard library on Linux. Setting this macro redirects
  # calls from "iconv*()" to "libiconv*()" by defining macros in the
  # C headers of "libiconv-1.15".
  add_definitions(-DLIBICONV_PLUG=1)

  # https://groups.google.com/d/msg/android-ndk/AS1nkxnk6m4/EQm09hD1tigJ
  add_definitions(
    -DBUILDING_LIBICONV=1
    -DIN_LIBRARY=1
    -DLIBDIR=""
    -DICONV_CONST=
    #-DENABLE_EXTRA=1
    )

  configure_file(
    ${LIBICONV_SOURCES_DIR}/srclib/localcharset.h
    ${LIBICONV_SOURCES_DIR}/include
    COPYONLY)

  set(HAVE_VISIBILITY 0)
  set(ICONV_CONST ${ICONV_CONST})
  set(USE_MBSTATE_T 1)
  set(BROKEN_WCHAR_H 0)
  set(EILSEQ)
  set(HAVE_WCHAR_T 1)
  configure_file(
    ${LIBICONV_SOURCES_DIR}/include/iconv.h.build.in
    ${LIBICONV_SOURCES_DIR}/include/iconv.h
    )
  unset(HAVE_VISIBILITY)
  unset(ICONV_CONST)
  unset(USE_MBSTATE_T)
  unset(BROKEN_WCHAR_H)
  unset(EILSEQ)
  unset(HAVE_WCHAR_T)

  if (NOT EXISTS ${LIBICONV_SOURCES_DIR}/include/config.h)
    # Create an empty "config.h" for libiconv
    file(WRITE ${LIBICONV_SOURCES_DIR}/include/config.h "")
  endif()

  include_directories(
    ${LIBICONV_SOURCES_DIR}/include
    )

  set(LIBICONV_SOURCES
    ${LIBICONV_SOURCES_DIR}/lib/iconv.c  
    ${LIBICONV_SOURCES_DIR}/lib/relocatable.c
    ${LIBICONV_SOURCES_DIR}/libcharset/lib/localcharset.c  
    ${LIBICONV_SOURCES_DIR}/libcharset/lib/relocatable.c
    )

  source_group(ThirdParty\\libiconv REGULAR_EXPRESSION ${LIBICONV_SOURCES_DIR}/.*)

  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_definitions(-DHAVE_WORKING_O_NOFOLLOW=0)
  else()
    add_definitions(-DHAVE_WORKING_O_NOFOLLOW=1)
  endif()

else() 
  CHECK_INCLUDE_FILE_CXX(iconv.h HAVE_ICONV_H)
  if (NOT HAVE_ICONV_H)
    message(FATAL_ERROR "Please install the libiconv-dev package")
  endif()

  # Check whether the support for libiconv is bundled within the
  # standard C library
  CHECK_FUNCTION_EXISTS(iconv_open HAVE_ICONV_LIB)
  if (NOT HAVE_ICONV_LIB)
    # No builtin support for libiconv, try and find an external library.
    # Open question: Does this make sense on any platform?
    CHECK_LIBRARY_EXISTS(iconv iconv_open "" HAVE_ICONV_LIB_2)
    if (NOT HAVE_ICONV_LIB_2)
      message(FATAL_ERROR "Please install the libiconv-dev package")
    else()
      link_libraries(iconv)
    endif()
  endif()
endif()
