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


cmake_minimum_required(VERSION 2.8)

project(Sample)

include(${CMAKE_SOURCE_DIR}/../../CMake/OrthancFrameworkParameters.cmake)

set(ENABLE_ZLIB ON)
set(ENABLE_WEB_SERVER ON)

include(${CMAKE_SOURCE_DIR}/../../CMake/OrthancFrameworkConfiguration.cmake)

add_executable(Sample
  ${ORTHANC_CORE_SOURCES}
  Sample.cpp
  )
