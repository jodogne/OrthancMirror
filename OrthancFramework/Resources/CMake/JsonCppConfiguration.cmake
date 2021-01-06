# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
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


set(JSONCPP_CXX11 OFF)

if (STATIC_BUILD OR NOT USE_SYSTEM_JSONCPP)
  if (USE_LEGACY_JSONCPP)
    set(JSONCPP_SOURCES_DIR ${CMAKE_BINARY_DIR}/jsoncpp-0.10.7)
    set(JSONCPP_URL "http://orthanc.osimis.io/ThirdPartyDownloads/jsoncpp-0.10.7.tar.gz")
    set(JSONCPP_MD5 "3a8072ca6a1fa9cbaf7715ae625f134f")
    add_definitions(-DORTHANC_LEGACY_JSONCPP=1)
  else()
    set(JSONCPP_SOURCES_DIR ${CMAKE_BINARY_DIR}/jsoncpp-1.9.4)
    set(JSONCPP_URL "http://orthanc.osimis.io/ThirdPartyDownloads/jsoncpp-1.9.4.tar.gz")
    set(JSONCPP_MD5 "4757b26ec89798c5247fa638edfdc446")
    add_definitions(-DORTHANC_LEGACY_JSONCPP=0)
    set(JSONCPP_CXX11 ON)
  endif()

  DownloadPackage(${JSONCPP_MD5} ${JSONCPP_URL} "${JSONCPP_SOURCES_DIR}")

  set(JSONCPP_SOURCES
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_reader.cpp
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_value.cpp
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_writer.cpp
    )

  include_directories(
    ${JSONCPP_SOURCES_DIR}/include
    )

  if (NOT ENABLE_LOCALE)
    add_definitions(-DJSONCPP_NO_LOCALE_SUPPORT=1)
  endif()
    
  source_group(ThirdParty\\JsonCpp REGULAR_EXPRESSION ${JSONCPP_SOURCES_DIR}/.*)

else()
  find_path(JSONCPP_INCLUDE_DIR json/reader.h
    /usr/include/jsoncpp
    /usr/local/include/jsoncpp
    )

  message("JsonCpp include dir: ${JSONCPP_INCLUDE_DIR}")
  include_directories(${JSONCPP_INCLUDE_DIR})
  link_libraries(jsoncpp)

  CHECK_INCLUDE_FILE_CXX(${JSONCPP_INCLUDE_DIR}/json/reader.h HAVE_JSONCPP_H)
  if (NOT HAVE_JSONCPP_H)
    message(FATAL_ERROR "Please install the libjsoncpp-dev package")
  endif()

  # Switch to the C++11 standard if the version of JsonCpp is 1.y.z
  if (EXISTS ${JSONCPP_INCLUDE_DIR}/json/version.h)
    file(STRINGS
      "${JSONCPP_INCLUDE_DIR}/json/version.h" 
      JSONCPP_VERSION_MAJOR1 REGEX
      ".*define JSONCPP_VERSION_MAJOR.*")

    if (NOT JSONCPP_VERSION_MAJOR1)
      message(FATAL_ERROR "Unable to extract the major version of JsonCpp")
    endif()
    
    string(REGEX REPLACE
      ".*JSONCPP_VERSION_MAJOR.*([0-9]+)$" "\\1" 
      JSONCPP_VERSION_MAJOR ${JSONCPP_VERSION_MAJOR1})
    message("JsonCpp major version: ${JSONCPP_VERSION_MAJOR}")

    if (JSONCPP_VERSION_MAJOR GREATER 0)
      set(JSONCPP_CXX11 ON)
    endif()
  else()
    message("Unable to detect the major version of JsonCpp, assuming < 1.0.0")
  endif()
endif()


if (JSONCPP_CXX11)
  # Osimis has encountered problems when this macro is left at its
  # default value (1000), so we increase this limit
  # https://gitlab.kitware.com/third-party/jsoncpp/commit/56df2068470241f9043b676bfae415ed62a0c172
  add_definitions(-DJSONCPP_DEPRECATED_STACK_LIMIT=5000)

  if (CMAKE_COMPILER_IS_GNUCXX)
    message("Switching to C++11 standard in gcc, as version of JsonCpp is >= 1.0.0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    message("Switching to C++11 standard in clang, as version of JsonCpp is >= 1.0.0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  endif()
endif()
