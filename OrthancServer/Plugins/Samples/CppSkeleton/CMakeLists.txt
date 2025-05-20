# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


cmake_minimum_required(VERSION 2.8)

project(OrthancSkeleton)

set(ORTHANC_PLUGIN_VERSION "mainline")
set(ORTHANC_PLUGIN_NAME "skeleton")

if (ORTHANC_PLUGIN_VERSION STREQUAL "mainline")
  set(ORTHANC_FRAMEWORK_DEFAULT_VERSION "mainline")
  set(ORTHANC_FRAMEWORK_DEFAULT_SOURCE "hg")
else()
  set(ORTHANC_FRAMEWORK_DEFAULT_VERSION "1.12.7")
  set(ORTHANC_FRAMEWORK_DEFAULT_SOURCE "web")
endif()


# Parameters of the build
set(STATIC_BUILD OFF CACHE BOOL "Static build of the third-party libraries (necessary for Windows)")
set(STANDALONE_BUILD ON CACHE BOOL "Standalone build (all the resources are embedded, necessary for releases)")
set(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")
set(ORTHANC_FRAMEWORK_SOURCE "${ORTHANC_FRAMEWORK_DEFAULT_SOURCE}" CACHE STRING "Source of the Orthanc framework (can be \"system\", \"hg\", \"archive\", \"web\" or \"path\")")
set(ORTHANC_FRAMEWORK_VERSION "${ORTHANC_FRAMEWORK_DEFAULT_VERSION}" CACHE STRING "Version of the Orthanc framework")
set(ORTHANC_FRAMEWORK_ARCHIVE "" CACHE STRING "Path to the Orthanc archive, if ORTHANC_FRAMEWORK_SOURCE is \"archive\"")
set(ORTHANC_FRAMEWORK_ROOT "" CACHE STRING "Path to the Orthanc source directory, if ORTHANC_FRAMEWORK_SOURCE is \"path\"")


# Advanced parameters to fine-tune linking against system libraries
set(USE_SYSTEM_ORTHANC_SDK ON CACHE BOOL "Use the system version of the Orthanc plugin SDK")
set(ORTHANC_FRAMEWORK_STATIC OFF CACHE BOOL "If linking against the Orthanc framework system library, indicates whether this library was statically linked")
mark_as_advanced(ORTHANC_FRAMEWORK_STATIC)


# Download and setup the Orthanc framework
if (ORTHANC_FRAMEWORK_SOURCE STREQUAL "path")
  include(${ORTHANC_FRAMEWORK_ROOT}/../Resources/CMake/DownloadOrthancFramework.cmake)
else()
  include(${CMAKE_SOURCE_DIR}/Resources/Orthanc/CMake/DownloadOrthancFramework.cmake)
endif()


if (ORTHANC_FRAMEWORK_SOURCE STREQUAL "system")
  if (ORTHANC_FRAMEWORK_USE_SHARED)
    include(FindBoost)
    find_package(Boost COMPONENTS filesystem regex thread)

    if (NOT Boost_FOUND)
      message(FATAL_ERROR "Unable to locate Boost on this system")
    endif()

    link_libraries(${Boost_LIBRARIES} jsoncpp)
  endif()

  link_libraries(${ORTHANC_FRAMEWORK_LIBRARIES})
else()
  include(${ORTHANC_FRAMEWORK_ROOT}/../Resources/CMake/OrthancFrameworkParameters.cmake)

  set(ENABLE_LOCALE OFF)         # Disable support for locales (notably in Boost)
  set(ENABLE_MODULE_JOBS OFF CACHE INTERNAL "")
  set(ENABLE_MODULE_DICOM ON CACHE INTERNAL "")

  include(${ORTHANC_FRAMEWORK_ROOT}/../Resources/CMake/OrthancFrameworkConfiguration.cmake)
  include_directories(${ORTHANC_FRAMEWORK_ROOT})
endif()


if (ORTHANC_FRAMEWORK_SOURCE STREQUAL "path")
  include(${ORTHANC_FRAMEWORK_ROOT}/../../OrthancServer/Plugins/Samples/Common/OrthancPluginsExports.cmake)
  include_directories(${ORTHANC_FRAMEWORK_ROOT}/../../OrthancServer/Plugins/Samples/Common/)

  set(ORTHANC_PLUGINS_SOURCES
    ${ORTHANC_FRAMEWORK_ROOT}/../../OrthancServer/Plugins/Samples/Common/OrthancPluginCppWrapper.cpp
    )
else()
  include(${CMAKE_SOURCE_DIR}/Resources/Orthanc/Plugins/OrthancPluginsExports.cmake)
  include_directories(${CMAKE_SOURCE_DIR}/Resources/Orthanc/Plugins/)

  set(ORTHANC_PLUGINS_SOURCES
    ${CMAKE_SOURCE_DIR}/Resources/Orthanc/Plugins/OrthancPluginCppWrapper.cpp
    )
endif()


# Check that the Orthanc SDK headers are available
if (STATIC_BUILD OR NOT USE_SYSTEM_ORTHANC_SDK)
  if (ORTHANC_FRAMEWORK_SOURCE STREQUAL "path")
    include_directories(${ORTHANC_FRAMEWORK_ROOT}/../../OrthancServer/Plugins/Include)
  else()
    include_directories(${CMAKE_SOURCE_DIR}/Resources/Orthanc/Sdk-1.0.0)
  endif()
else ()
  CHECK_INCLUDE_FILE_CXX(orthanc/OrthancCPlugin.h HAVE_ORTHANC_H)
  if (NOT HAVE_ORTHANC_H)
    message(FATAL_ERROR "Please install the headers of the Orthanc plugins SDK")
  endif()
endif()


if (STANDALONE_BUILD)
  add_definitions(
    -DORTHANC_STANDALONE=1
    )
  set(EMBEDDED_RESOURCES
    PLUGIN_RESOURCES ${CMAKE_SOURCE_DIR}/EmbeddedResources/
    )
else()
  add_definitions(
    -DORTHANC_STANDALONE=0
    -DPLUGIN_RESOURCES_PATH="${CMAKE_SOURCE_DIR}/EmbeddedResources/"
    )
endif()


message("Setting the version of the library to ${ORTHANC_PLUGIN_VERSION}")

add_definitions(
  -DHAS_ORTHANC_EXCEPTION=1
  -DORTHANC_PLUGIN_NAME="${ORTHANC_PLUGIN_NAME}"
  -DORTHANC_PLUGIN_VERSION="${ORTHANC_PLUGIN_VERSION}"
  )

EmbedResources(
  ${EMBEDDED_RESOURCES}
  )

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  link_libraries(rt)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  SET(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lws2_32")

  execute_process(
    COMMAND
    ${PYTHON_EXECUTABLE} ${ORTHANC_FRAMEWORK_ROOT}/../Resources/WindowsResources.py
    ${ORTHANC_PLUGIN_VERSION} "OrthancSkeleton" OrthancSkeleton.dll "Skeleton plugin for Orthanc"
    ERROR_VARIABLE Failure
    OUTPUT_FILE ${AUTOGENERATED_DIR}/Version.rc
    )

  if (Failure)
    message(FATAL_ERROR "Error while computing the version information: ${Failure}")
  endif()

  list(APPEND AUTOGENERATED_SOURCES  ${AUTOGENERATED_DIR}/Version.rc)
endif()


add_custom_target(
  AutogeneratedTarget
  DEPENDS
  ${AUTOGENERATED_SOURCES}
  )

add_library(OrthancSkeleton
  SHARED
  ${AUTOGENERATED_SOURCES}
  ${ORTHANC_CORE_SOURCES}
  ${ORTHANC_PLUGINS_SOURCES}

  # Your source files
  ${CMAKE_SOURCE_DIR}/Plugin.cpp
  )

add_dependencies(OrthancSkeleton AutogeneratedTarget)


set_target_properties(OrthancSkeleton PROPERTIES
  VERSION ${ORTHANC_PLUGIN_VERSION}
  SOVERSION ${ORTHANC_PLUGIN_VERSION})

install(
  TARGETS OrthancSkeleton
  RUNTIME DESTINATION lib    # Destination for Windows
  LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
  )

if (COMMAND DefineSourceBasenameForTarget)
  DefineSourceBasenameForTarget(OrthancSkeleton)
endif()
