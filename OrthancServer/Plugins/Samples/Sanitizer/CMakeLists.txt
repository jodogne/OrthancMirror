# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2024 Osimis S.A., Belgium
# Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

project(Sanitizer)

SET(STATIC_BUILD OFF CACHE BOOL "Static build of the third-party libraries (necessary for Windows)")
SET(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")

SET(USE_SYSTEM_JSONCPP ON CACHE BOOL "Use the system version of JsonCpp")
SET(USE_SYSTEM_BOOST ON CACHE BOOL "Use the system version of boost")
SET(ORTHANC_FRAMEWORK_SOURCE path)
SET(ORTHANC_FRAMEWORK_ROOT ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Sources)

include(${CMAKE_SOURCE_DIR}/../Common/OrthancPlugins.cmake)
include(${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Resources/CMake/JsonCppConfiguration.cmake)
include(${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Resources/CMake/BoostConfiguration.cmake)

include(${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Resources/CMake/DownloadOrthancFramework.cmake)
include(${ORTHANC_FRAMEWORK_ROOT}/../Resources/CMake/OrthancFrameworkParameters.cmake)
set(ENABLE_LOCALE ON)
set(ENABLE_DCMTK ON)
include(${ORTHANC_FRAMEWORK_ROOT}/../Resources/CMake/OrthancFrameworkConfiguration.cmake)
include_directories(${ORTHANC_FRAMEWORK_ROOT})


add_library(Sanitizer SHARED 
    ${CMAKE_SOURCE_DIR}/../Common/OrthancPluginCppWrapper.cpp
    ${JSONCPP_SOURCES}
    ${BOOST_SOURCES}
    Plugin.cpp
    ${ORTHANC_CORE_SOURCES}
    ${ORTHANC_DICOM_SOURCES}
    )

DefineSourceBasenameForTarget(Sanitizer)

target_link_libraries(Sanitizer ${DCMTK_LIBRARIES})


install(
  TARGETS Sanitizer
  RUNTIME DESTINATION lib    # Destination for Windows
  LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
  )
