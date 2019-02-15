cmake_minimum_required(VERSION 2.8)
project(IcuCodeGeneration)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include(${CMAKE_SOURCE_DIR}/../../CMake/Compiler.cmake)
include(${CMAKE_SOURCE_DIR}/../../CMake/DownloadPackage.cmake)
include(Version.cmake)

set(ALLOW_DOWNLOADS ON)
DownloadPackage(${LIBICU_MD5} ${LIBICU_URL} "${LIBICU_SOURCES_DIR}")

include_directories(
  ${LIBICU_SOURCES_DIR}/source/common
  ${LIBICU_SOURCES_DIR}/source/i18n
  ${LIBICU_SOURCES_DIR}/source/tools/toolutil/
  )

aux_source_directory(${LIBICU_SOURCES_DIR}/source/common         LIBICU_SOURCES)
aux_source_directory(${LIBICU_SOURCES_DIR}/source/i18n           LIBICU_SOURCES)
aux_source_directory(${LIBICU_SOURCES_DIR}/source/tools/toolutil LIBICU_SOURCES)

set_source_files_properties(
  ${LIBICU_SOURCES_DIR}/source/tools/genccode/genccode.c
  PROPERTIES COMPILE_DEFINITIONS "char16_t=uint16_t"
  )

add_executable(IcuCodeGeneration
  ${LIBICU_SOURCES_DIR}/source/tools/genccode/genccode.c
  ${LIBICU_SOURCES_DIR}/source/stubdata/stubdata.cpp
  ${LIBICU_SOURCES}
  )

add_custom_command(
  OUTPUT   ${LIBICU_DATA}
  COMMAND  IcuCodeGeneration ${LIBICU_SOURCE_DATA}
  DEPENDS  IcuCodeGeneration
  )

add_custom_command(
  OUTPUT   ${LIBICU_DATA}.gz
  COMMAND  gzip -f ${LIBICU_DATA}
  DEPENDS  ${LIBICU_DATA}
  )

add_custom_target(Final ALL DEPENDS  ${LIBICU_DATA}.gz)

install(
  FILES ${CMAKE_BINARY_DIR}/${LIBICU_DATA}.gz
  DESTINATION ${CMAKE_SOURCE_DIR}/../../../ThirdPartyDownloads
  )

add_definitions(
  #-DU_COMBINED_IMPLEMENTATION
  -DUCONFIG_NO_SERVICE=1
  -DU_COMMON_IMPLEMENTATION
  -DU_ENABLE_DYLOAD=0
  -DU_HAVE_STD_STRING=1
  -DU_I18N_IMPLEMENTATION
  -DU_IO_IMPLEMENTATION
  -DU_STATIC_IMPLEMENTATION=1
  -DU_TOOLUTIL_IMPLEMENTATION
  )
