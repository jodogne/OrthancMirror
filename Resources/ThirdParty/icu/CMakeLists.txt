cmake_minimum_required(VERSION 2.8)
project(IcuCodeGeneration)

set(USE_LEGACY_LIBICU OFF CACHE BOOL "Use icu icu4c-58_2, latest version not requiring a C++11 compiler (for LSB and old versions of Visual Studio)")

if (NOT USE_LEGACY_LIBICU)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

include(${CMAKE_SOURCE_DIR}/../../CMake/Compiler.cmake)
include(${CMAKE_SOURCE_DIR}/../../CMake/DownloadPackage.cmake)
include(Version.cmake)

set(SOURCE_DATA
  "${LIBICU_SOURCES_DIR}/source/data/in/${LIBICU_DATA_VERSION}${LIBICU_SUFFIX}.dat")

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

if (USE_LEGACY_LIBICU)
  list(APPEND LIBICU_SOURCES
    ${LIBICU_SOURCES_DIR}/source/stubdata/stubdata.c
    )
else()
  list(APPEND LIBICU_SOURCES
    ${LIBICU_SOURCES_DIR}/source/stubdata/stubdata.cpp
    )
  set_source_files_properties(
    ${LIBICU_SOURCES_DIR}/source/tools/genccode/genccode.c
    PROPERTIES COMPILE_DEFINITIONS "char16_t=uint16_t"
    )
endif()



add_executable(IcuCodeGeneration
  ${LIBICU_SOURCES_DIR}/source/tools/genccode/genccode.c
  ${LIBICU_SOURCES}
  )

configure_file(${SOURCE_DATA}
  ${CMAKE_BINARY_DIR}/${LIBICU_DATA_VERSION}.dat
  COPYONLY)

add_custom_command(
  OUTPUT   ${CMAKE_BINARY_DIR}/${LIBICU_DATA}
  COMMAND  IcuCodeGeneration ${CMAKE_BINARY_DIR}/${LIBICU_DATA_VERSION}.dat
  DEPENDS  IcuCodeGeneration
  )

# "--no-name" is necessary for 7-zip on Windows to behave similarly to gunzip
add_custom_command(
  OUTPUT   ${CMAKE_BINARY_DIR}/${LIBICU_DATA}.gz
  COMMAND  gzip ${CMAKE_BINARY_DIR}/${LIBICU_DATA_VERSION}_dat.c --no-name -c > ${CMAKE_BINARY_DIR}/${LIBICU_DATA}.gz
  DEPENDS  ${LIBICU_DATA}
  )


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Generate a precompiled version for Visual Studio 64bit
  set(TMP_ASSEMBLER      ${CMAKE_BINARY_DIR}/${LIBICU_DATA_VERSION}_dat.S)
  set(TMP_OBJECT         ${CMAKE_BINARY_DIR}/${LIBICU_DATA_VERSION}_dat-x86_64-mingw32.o)
  set(TMP_LIBRARY        ${CMAKE_BINARY_DIR}/${LIBICU_DATA_VERSION}_dat-x86_64-mingw32.lib)
  set(PRECOMPILED_WIN64  ${CMAKE_BINARY_DIR}/${LIBICU_DATA_VERSION}_dat-x86_64-mingw32.lib.gz)
  
  add_custom_command(
    OUTPUT   ${TMP_ASSEMBLER}
    COMMAND  IcuCodeGeneration ${CMAKE_BINARY_DIR}/${LIBICU_DATA_VERSION}.dat --assembly gcc-mingw64
    DEPENDS  IcuCodeGeneration
    )

  add_custom_command(
    OUTPUT   ${TMP_OBJECT}
    COMMAND  x86_64-w64-mingw32-gcc -c ${TMP_ASSEMBLER} -o ${TMP_OBJECT}
    DEPENDS  ${TMP_ASSEMBLER}
    )

  add_custom_command(
    OUTPUT   ${TMP_LIBRARY}
    COMMAND  x86_64-w64-mingw32-ar qc ${TMP_LIBRARY} ${TMP_OBJECT}
    COMMAND  x86_64-w64-mingw32-ranlib ${TMP_LIBRARY}
    DEPENDS  ${TMP_OBJECT}
    )

  # "--no-name" is necessary for 7-zip on Windows to behave similarly to gunzip
  add_custom_command(
    OUTPUT   ${PRECOMPILED_WIN64}
    COMMAND  gzip ${TMP_LIBRARY} --no-name -c > ${PRECOMPILED_WIN64}
    DEPENDS  ${TMP_LIBRARY}
    )
endif()


add_custom_target(Final ALL DEPENDS
  ${CMAKE_BINARY_DIR}/${LIBICU_DATA}.gz
  ${PRECOMPILED_WIN64}
  )

install(
  FILES
  ${CMAKE_BINARY_DIR}/${LIBICU_DATA}.gz
  ${PRECOMPILED_WIN64}
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
