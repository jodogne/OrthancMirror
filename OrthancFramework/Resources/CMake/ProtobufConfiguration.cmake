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


if (STATIC_BUILD OR NOT USE_SYSTEM_PROTOBUF)
  if (ENABLE_PROTOBUF_COMPILER)
    include(ExternalProject)
    externalproject_add(ProtobufCompiler
      SOURCE_DIR "${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/ProtocolBuffers"
      BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ProtobufCompiler-build"
      # this helps triggering build when changing the external project
      BUILD_ALWAYS 1
      CMAKE_ARGS
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
      INSTALL_COMMAND ""
      )
    set(PROTOC_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ProtobufCompiler-build/protoc)
  endif()

  include(${CMAKE_CURRENT_LIST_DIR}/../ProtocolBuffers/ProtobufLibrary.cmake)  
  source_group(ThirdParty\\Protobuf REGULAR_EXPRESSION ${PROTOBUF_SOURCE_DIR}/.*)

else()
  if (CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "If cross-compiling, the static version of Protocol Buffers should be used to avoid version mismatch")
  endif()
  
  if (ENABLE_PROTOBUF_COMPILER)
    find_program(PROTOC_EXECUTABLE protoc)
    if (${PROTOC_EXECUTABLE} MATCHES "PROTOC_EXECUTABLE-NOTFOUND")
      message(FATAL_ERROR "Please install the 'protoc' compiler for Protocol Buffers (package 'protobuf-compiler' on Debian/Ubuntu)")
    endif()
    add_custom_target(ProtobufCompiler)
  endif()
  
  check_include_file_cxx(google/protobuf/any.h HAVE_PROTOBUF_H)
  if (NOT HAVE_PROTOBUF_H)
    message(FATAL_ERROR "Please install the libprotobuf-dev package")
  endif()

  set(CMAKE_REQUIRED_LIBRARIES "protobuf")

  include(CheckCXXSourceCompiles) 
  check_cxx_source_compiles(
    "
#include <google/protobuf/descriptor.h>
int main()
{
  google::protobuf::FieldDescriptor::TypeName(google::protobuf::FieldDescriptor::TYPE_FLOAT);
}
"  HAVE_PROTOBUF_LIB)
  if (NOT HAVE_PROTOBUF_LIB)
    message(FATAL_ERROR "Cannot find the protobuf library")
  endif()
  
  unset(CMAKE_REQUIRED_LIBRARIES)

  link_libraries(protobuf)
endif()
