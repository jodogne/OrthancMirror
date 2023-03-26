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


cmake_minimum_required(VERSION 2.8.3)

project(ProtocolBuffers)

set(ALLOW_DOWNLOADS ON)

include(${CMAKE_SOURCE_DIR}/../CMake/DownloadPackage.cmake)
include(${CMAKE_SOURCE_DIR}/../CMake/Compiler.cmake)

include(${CMAKE_SOURCE_DIR}/ProtobufLibrary.cmake)

set(PROTOBUF_COMPILER_SOURCES
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/code_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/command_line_interface.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_enum.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_enum_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_extension.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_file.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_helpers.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_map_field.cc  
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_message.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_message_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_padding_optimizer.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_primitive_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_service.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/cpp/cpp_string_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_doc_comment.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_enum.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_enum_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_field_base.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_helpers.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_map_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_message.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_message_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_primitive_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_reflection_class.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_repeated_enum_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_repeated_message_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_repeated_primitive_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_source_generator_base.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/csharp/csharp_wrapper_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/importer.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_context.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_doc_comment.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_enum.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_enum_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_enum_field_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_enum_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_extension.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_extension_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_file.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_generator_factory.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_helpers.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_lazy_message_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_lazy_message_field_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_map_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_map_field_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_message.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_message_builder.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_message_builder_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_message_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_message_field_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_message_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_name_resolver.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_primitive_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_primitive_field_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_service.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_shared_code_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_string_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/java/java_string_field_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_enum.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_enum_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_extension.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_file.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_helpers.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_map_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_message.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_message_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/javanano/javanano_primitive_field.cc
  #${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/js/embed.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/js/js_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/js/well_known_types_embed.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/main.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_enum.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_enum_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_extension.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_file.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_helpers.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_map_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_message.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_message_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_oneof.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/objectivec/objectivec_primitive_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/parser.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/php/php_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/plugin.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/plugin.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/python/python_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/ruby/ruby_generator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/subprocess.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/compiler/zip_writer.cc
  )

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set_property(
    SOURCE ${PROTOBUF_COMPILER_SOURCES}
    PROPERTY COMPILE_DEFINITIONS "HAVE_PTHREAD=1"
    )
endif()

add_executable(protoc
  ${PROTOBUF_LIBRARY_SOURCES}
  ${PROTOBUF_COMPILER_SOURCES}
  )
