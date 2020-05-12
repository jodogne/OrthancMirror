# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2020 Osimis S.A., Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Affero General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Affero General Public License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


cmake_minimum_required(VERSION 2.8)

project(GdcmDecoder)

SET(PLUGIN_VERSION "mainline" CACHE STRING "Version of the plugin")


# Parameters of the build
set(STATIC_BUILD OFF CACHE BOOL "Static build of the third-party libraries (necessary for Windows)")
set(STANDALONE_BUILD ON CACHE BOOL "Standalone build (all the resources are embedded, necessary for releases)")
set(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")

# Advanced parameters to fine-tune linking against system libraries
set(USE_SYSTEM_GDCM ON CACHE BOOL "Use the system version of Grassroot DICOM (GDCM)")
set(USE_SYSTEM_ORTHANC_SDK ON CACHE BOOL "Use the system version of the Orthanc plugin SDK")

# Setup the Orthanc framework
set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../..)

set(ORTHANC_FRAMEWORK_PLUGIN ON)
include(${ORTHANC_ROOT}/Resources/CMake/OrthancFrameworkParameters.cmake)

set(ENABLE_LOCALE OFF CACHE INTERNAL "")      # Disable support for locales (notably in Boost)
set(ENABLE_MODULE_IMAGES OFF CACHE INTERNAL "")
set(ENABLE_MODULE_JOBS OFF CACHE INTERNAL "")

include(${ORTHANC_ROOT}/Resources/CMake/OrthancFrameworkConfiguration.cmake)

include(GdcmConfiguration.cmake)


# Check that the Orthanc SDK headers are available
if (STATIC_BUILD OR NOT USE_SYSTEM_ORTHANC_SDK)
  #include_directories(${CMAKE_SOURCE_DIR}/Resources/Orthanc/Sdk-0.9.5)
  include_directories(${CMAKE_SOURCE_DIR}/../../Include) # TODO => SYNC 0.9.5
else ()
  CHECK_INCLUDE_FILE_CXX(orthanc/OrthancCPlugin.h HAVE_ORTHANC_H)
  if (NOT HAVE_ORTHANC_H)
    message(FATAL_ERROR "Please install the headers of the Orthanc plugins SDK")
  endif()
endif()


include_directories(${ORTHANC_ROOT})

add_definitions(
  -DPLUGIN_VERSION="${PLUGIN_VERSION}"
  -DHAS_ORTHANC_EXCEPTION=1
  -DORTHANC_ENABLE_LOGGING_PLUGIN=1
  )

add_library(GdcmDecoder SHARED
  GdcmDecoderCache.cpp
  GdcmImageDecoder.cpp
  Plugin.cpp
  ${CMAKE_SOURCE_DIR}/../Common/OrthancPluginCppWrapper.cpp
  ${ORTHANC_CORE_SOURCES}
  )

target_link_libraries(GdcmDecoder ${GDCM_LIBRARIES})

if (STATIC_BUILD OR NOT USE_SYSTEM_GDCM)
  add_dependencies(GdcmDecoder GDCM)
endif()
