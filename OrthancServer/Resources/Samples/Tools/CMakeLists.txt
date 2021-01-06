# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# In addition, as a special exception, the copyright holders of this
# program give permission to link the code of its release with the
# OpenSSL project's "OpenSSL" library (or with modified versions of it
# that use the same license as the "OpenSSL" library), and distribute
# the linked executables. You must obey the GNU General Public License
# in all respects for all of the code used other than "OpenSSL". If you
# modify file(s) with this exception, you may extend this exception to
# your version of the file(s), but you are not obligated to do so. If
# you do not wish to do so, delete this exception statement from your
# version. If you delete this exception statement from all source files
# in the program, then also delete it here.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


cmake_minimum_required(VERSION 2.8)

project(OrthancTools)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Linking with "pthread" is necessary, otherwise the software crashes
  # http://sourceware.org/bugzilla/show_bug.cgi?id=10652#c17
  link_libraries(pthread dl)
endif()

include(${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Resources/CMake/OrthancFrameworkParameters.cmake)

set(ENABLE_ZLIB ON)
set(STATIC_BUILD ON)
set(ALLOW_DOWNLOADS ON)

include(${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Resources/CMake/OrthancFrameworkConfiguration.cmake)

add_executable(RecoverCompressedFile
  RecoverCompressedFile.cpp

  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Resources/ThirdParty/base64/base64.cpp
  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Resources/ThirdParty/md5/md5.c
  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Sources/Compression/DeflateBaseCompressor.cpp
  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Sources/Compression/ZlibCompressor.cpp
  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Sources/Enumerations.cpp
  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Sources/Logging.cpp
  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Sources/SystemToolbox.cpp
  ${CMAKE_SOURCE_DIR}/../../../../OrthancFramework/Sources/Toolbox.cpp

  ${BOOST_SOURCES}
  ${JSONCPP_SOURCES}
  ${UUID_SOURCES}
  ${ZLIB_SOURCES}
  )
