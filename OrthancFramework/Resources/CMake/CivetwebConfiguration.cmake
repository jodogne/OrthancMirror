# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2020 Osimis S.A., Belgium
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


if (STATIC_BUILD OR NOT USE_SYSTEM_CIVETWEB)

  ## WARNING: "civetweb-1.12.tar.gz" comes with a subfolder
  ## "civetweb-1.12/test/nonlatin" that cannot be removed by "hg purge
  ## --all" on Windows hosts. We thus created a custom
  ## "civetweb-1.12-fixed.tar.gz" as follows:
  ##
  ##  $ cd /tmp
  ##  $ wget http://orthanc.osimis.io/ThirdPartyDownloads/civetweb-1.12.tar.gz
  ##  $ tar xvf civetweb-1.12.tar.gz
  ##  $ rm -rf civetweb-1.12/src/third_party/ civetweb-1.12/test/
  ##  $ tar cvfz civetweb-1.12-fixed.tar.gz civetweb-1.12
  ##
  
  set(CIVETWEB_SOURCES_DIR ${CMAKE_BINARY_DIR}/civetweb-1.12)
  set(CIVETWEB_URL "http://orthanc.osimis.io/ThirdPartyDownloads/civetweb-1.12-fixed.tar.gz")
  set(CIVETWEB_MD5 "016ed7cd26cbc46b5941f0cbfb2e4ac8")

  if (IS_DIRECTORY "${CIVETWEB_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()

  DownloadPackage(${CIVETWEB_MD5} ${CIVETWEB_URL} "${CIVETWEB_SOURCES_DIR}")

  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/civetweb-1.12.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (FirstRun AND Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()
  
  include_directories(
    ${CIVETWEB_SOURCES_DIR}/include
    )

  set(CIVETWEB_SOURCES
    ${CIVETWEB_SOURCES_DIR}/src/civetweb.c
    )

  # New in Orthanc 1.6.0: Enable support of compression in civetweb
  set_source_files_properties(
    ${CIVETWEB_SOURCES}
    PROPERTIES COMPILE_DEFINITIONS
    "USE_ZLIB=1")
  
  if (ENABLE_SSL)
    add_definitions(
      -DNO_SSL_DL=1
      )
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
        ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
      link_libraries(dl)
    endif()

  else()
    add_definitions(
      -DNO_SSL=1   # Remove SSL support from civetweb
      )
  endif()

  source_group(ThirdParty\\Civetweb REGULAR_EXPRESSION ${CIVETWEB_SOURCES_DIR}/.*)

  add_definitions(
    -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=1
    )

else()
  CHECK_INCLUDE_FILE_CXX(civetweb.h HAVE_CIVETWEB_H)
  if (NOT HAVE_CIVETWEB_H)
    message(FATAL_ERROR "Please install the libcivetweb-devel package")
  endif()

  cmake_reset_check_state()
  set(CMAKE_REQUIRED_LIBRARIES dl pthread)
  CHECK_LIBRARY_EXISTS(civetweb mg_start "" HAVE_CIVETWEB_LIB)
  if (NOT HAVE_CIVETWEB_LIB)
    message(FATAL_ERROR "Please install the libcivetweb-devel package")
  endif()

  link_libraries(civetweb)
  unset(CMAKE_REQUIRED_LIBRARIES)

  add_definitions(
    -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=0
    )
endif()
