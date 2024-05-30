# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


set(PROTOBUF_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/protobuf-3.5.1)

if (IS_DIRECTORY "${PROTOBUF_SOURCE_DIR}")
  set(FirstRun OFF)
else()
  set(FirstRun ON)
endif()

DownloadPackage(
  "ca0d9b243e649d398a6b419acd35103a"
  "https://orthanc.uclouvain.be/downloads/third-party-downloads/protobuf-cpp-3.5.1.tar.gz"
  "${CMAKE_CURRENT_BINARY_DIR}/protobuf-3.5.1")

if (FirstRun)
  # Apply the patches
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/protobuf-3.5.1.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()
endif()

include_directories(
  ${PROTOBUF_SOURCE_DIR}/src
  )
  
set(PROTOBUF_LIBRARY_SOURCES
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/any.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/any.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/api.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/arena.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/arenastring.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/descriptor.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/descriptor.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/descriptor_database.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/duration.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/dynamic_message.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/empty.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/extension_set.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/extension_set_heavy.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/field_mask.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/generated_message_reflection.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/generated_message_table_driven.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/generated_message_table_driven_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/generated_message_util.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/io/coded_stream.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/io/gzip_stream.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/io/printer.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/io/strtod.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/io/tokenizer.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/io/zero_copy_stream.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/io/zero_copy_stream_impl.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/io/zero_copy_stream_impl_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/map_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/message.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/message_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/reflection_ops.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/repeated_field.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/service.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/source_context.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/struct.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_arm64_gcc.h
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_arm_gcc.h
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_generic_gcc.h
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_mips_gcc.h
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_ppc_gcc.h
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_x86_gcc.h
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/common.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/int128.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/io_win32.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/mathlimits.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/once.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/status.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/statusor.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/stringpiece.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/stringprintf.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/structurally_valid.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/strutil.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/substitute.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/time.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/stubs/bytestream.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/text_format.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/timestamp.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/type.pb.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/unknown_field_set.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/delimited_message_util.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/field_comparator.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/field_mask_util.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/datapiece.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/default_value_objectwriter.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/error_listener.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/field_mask_utility.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/json_escaping.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/json_objectwriter.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/json_stream_parser.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/object_writer.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/proto_writer.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/protostream_objectsource.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/protostream_objectwriter.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/type_info.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/internal/utility.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/json_util.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/message_differencer.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/time_util.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/util/type_resolver_util.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/wire_format.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/wire_format_lite.cc
  ${PROTOBUF_SOURCE_DIR}/src/google/protobuf/wrappers.pb.cc
  )

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set_property(
    SOURCE ${PROTOBUF_LIBRARY_SOURCES} APPEND
    PROPERTY COMPILE_DEFINITIONS "HAVE_PTHREAD=1"
    )
endif()
