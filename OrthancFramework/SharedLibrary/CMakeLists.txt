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



## To see all the exported symbols in the DLL:
##
##  $ i686-w64-mingw32-objdump -p ./libOrthancFramework.dll
##
## IMPORTANT: "-static-libgcc" prevents catching exception in the
## .EXE, which makes throwing exceptions crash the software!
##


cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0058 NEW)

project(OrthancFramework)



#####################################################################
## Additional parameters
#####################################################################

# *Do not* use CMAKE_INSTALL_PREFIX, otherwise CMake automatically
# adds CMAKE_INSTALL_PREFIX to the include_directories(), which causes
# issues if re-building the shared library after install!
set(ORTHANC_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE PATH "")
SET(UNIT_TESTS_WITH_HTTP_CONNEXIONS ON CACHE BOOL "Allow unit tests to make HTTP requests")
set(BUILD_SHARED_LIBRARY ON CACHE BOOL "Whether to build a shared library instead of a static library")
set(ORTHANC_FRAMEWORK_ADDITIONAL_LIBRARIES "" CACHE STRING "Additional libraries to link against, separated by whitespaces, typically needed if using the static library (a typical value is \"uuid curl civetweb\")")



#####################################################################
## Configuration of the Orthanc framework
#####################################################################

# This must be before inclusion of "OrthancFrameworkParameters.cmake" to take effect
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND
    CMAKE_COMPILER_IS_GNUCXX) # MinGW
  set(DYNAMIC_MINGW_STDLIB ON)   # Disable static linking against libc (to throw exceptions)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
endif()

include(${CMAKE_SOURCE_DIR}/../Resources/CMake/OrthancFrameworkParameters.cmake)

# "ORTHANC_VERSION" is initialized by "OrthancFrameworkParameters.cmake"
set(ORTHANC_FRAMEWORK_SOVERSION "${ORTHANC_VERSION}" CACHE STRING "On GNU/Linux, the SOVERSION to be used for the shared library")

if (STATIC_BUILD OR NOT USE_SYSTEM_DCMTK)
  set(STANDALONE_BUILD ON)
else()
  set(STANDALONE_BUILD OFF)
endif()

set(ENABLE_DCMTK ON)
set(ENABLE_DCMTK_TRANSCODING ON)
set(ENABLE_GOOGLE_TEST ON)
set(ENABLE_JPEG ON)
set(ENABLE_LOCALE ON)
set(ENABLE_LUA ON)
set(ENABLE_PNG ON)
set(ENABLE_PUGIXML ON)
set(ENABLE_ZLIB ON)

if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  # WebAssembly or asm.js
  set(BOOST_LOCALE_BACKEND "libiconv")
  set(ORTHANC_SANDBOXED ON)
  set(STANDALONE_BUILD ON)

  # Will be used by "../Resources/CMake/EmscriptenParameters.cmake"
  set(WASM_FLAGS "-s SIDE_MODULE=1 -s EXPORT_ALL=1")

else()
  # Enable all the remaining modules for other targets
  set(ENABLE_CRYPTO_OPTIONS ON)
  set(ENABLE_DCMTK_NETWORKING ON)
  set(ENABLE_OPENSSL_ENGINES ON)
  set(ENABLE_SQLITE ON)
  set(ENABLE_WEB_CLIENT ON)
  set(ENABLE_WEB_SERVER ON)

  set(BOOST_LOCALE_BACKEND "icu")
  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    set(USE_LEGACY_JSONCPP ON)
    set(USE_LEGACY_LIBICU ON)
  endif()
endif()


set(ORTHANC_BUILDING_FRAMEWORK_LIBRARY ON)

include(${CMAKE_SOURCE_DIR}/../Resources/CMake/OrthancFrameworkConfiguration.cmake)



#####################################################################
## Configuration the visibility of the third-party libraries in the
## shared library
#####################################################################

if (STATIC_BUILD OR NOT USE_SYSTEM_JSONCPP)
  set(ORTHANC_STATIC_JSONCPP ON)
else()
  set(ORTHANC_STATIC_JSONCPP OFF)
endif()

if (STATIC_BUILD OR NOT USE_SYSTEM_BOOST)
  set(ORTHANC_STATIC_BOOST ON)
else()
  set(ORTHANC_STATIC_BOOST OFF)
endif()

if (STATIC_BUILD OR NOT USE_SYSTEM_SQLITE)
  set(ORTHANC_STATIC_SQLITE ON)
else()
  set(ORTHANC_STATIC_SQLITE OFF)
endif()

if (STATIC_BUILD OR NOT USE_SYSTEM_PUGIXML)
  set(ORTHANC_STATIC_PUGIXML ON)
else()
  set(ORTHANC_STATIC_PUGIXML OFF)
endif()

if (STATIC_BUILD OR NOT USE_SYSTEM_DCMTK)
  set(ORTHANC_STATIC_DCMTK ON)
else()
  set(ORTHANC_STATIC_DCMTK OFF)
endif()


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

  # Control the visibility of JsonCpp
  if (ORTHANC_STATIC_JSONCPP)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
      set_source_files_properties(${JSONCPP_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS "JSON_API=__declspec(dllexport)"
        )
      set(ORTHANC_JSON_API "__declspec(dllimport)")
    else()
      set(ORTHANC_JSON_API "__attribute__((visibility(\"default\")))")
      set_source_files_properties(${JSONCPP_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS "JSON_API=${ORTHANC_JSON_API}"
        )
    endif()
  endif()

  # Control the visibility of SQLite
  if (ORTHANC_STATIC_SQLITE)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
      set_source_files_properties(${SQLITE_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS "SQLITE_API=__declspec(dllexport)"
        )
      set(ORTHANC_SQLITE_API "__declspec(dllimport)")
    else()
      set(ORTHANC_SQLITE_API "__attribute__((visibility(\"default\")))")
      set_source_files_properties(${SQLITE_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS "SQLITE_API=${ORTHANC_SQLITE_API}"
        )
    endif()
  endif()

  # Control the visibility of Boost
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows" AND
      ORTHANC_STATIC_BOOST)
    set_source_files_properties(${ORTHANC_CORE_SOURCES_INTERNAL}
      PROPERTIES COMPILE_DEFINITIONS "BOOST_DATE_TIME_SOURCE;BOOST_FILESYSTEM_SOURCE;BOOST_LOCALE_SOURCE;BOOST_REGEX_SOURCE"
      )
  endif()

  # Control the visibility of Pugixml
  if (ORTHANC_STATIC_PUGIXML)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
      set_source_files_properties(${PUGIXML_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS "PUGIXML_API=__declspec(dllexport)"
        )
      set(ORTHANC_PUGIXML_API "__declspec(dllimport)")
    else()
      set(ORTHANC_PUGIXML_API "__attribute__((visibility(\"default\")))")
      set_source_files_properties(${PUGIXML_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS "PUGIXML_API=${ORTHANC_PUGIXML_API}"
        )
    endif()
  endif()

  # Control the visibility of DCMTK: We only export the "dcmdata" module
  if (ORTHANC_STATIC_DCMTK)
    set_source_files_properties(${DCMTK_SOURCES}
      PROPERTIES COMPILE_DEFINITIONS "DCMTK_BUILD_IN_PROGRESS;DCMTK_BUILD_SINGLE_SHARED_LIBRARY;DCMTK_SHARED;HAVE_HIDDEN_VISIBILITY;dcmdata_EXPORTS"
      )
  endif()
endif()


add_definitions(
  -DCIVETWEB_API=    # Don't export the public symbols from CivetWeb
  )



#####################################################################
## Building the shared library
#####################################################################

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  execute_process(
    COMMAND 
    ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../Resources/WindowsResources.py
    ${ORTHANC_VERSION} "OrthancFramework" OrthancFramework.dll "Shared library containing the Orthanc framework"
    ERROR_VARIABLE Failure
    OUTPUT_FILE ${AUTOGENERATED_DIR}/Version.rc
    )

  if (Failure)
    message(FATAL_ERROR "Error while computing the version information: ${Failure}")
  endif()

  list(APPEND AUTOGENERATED_SOURCES  ${AUTOGENERATED_DIR}/Version.rc)
endif()


# Those two files collide with each other, and thus are merged into a
# single "DllMain.cpp"
list(REMOVE_ITEM ORTHANC_CORE_SOURCES
  ${BOOST_SOURCES_DIR}/libs/thread/src/win32/tss_dll.cpp
  ${OPENSSL_SOURCES_DIR}/crypto/dllmain.c
  )

if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  # In WebAssembly, a SIDE_MODULE is an executable
  add_executable(OrthancFramework
    ${AUTOGENERATED_SOURCES}
    ${ORTHANC_CORE_SOURCES}
    ${ORTHANC_DICOM_SOURCES}
    )

  # CMake does not natively handle SIDE_MODULE, and believes that
  # Emscripten produces a ".js" file (whereas it creates only the
  # ".wasm"). Create a dummy ".js" for target to work.
  add_custom_command(
    TARGET OrthancFramework POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/OrthancFramework.js
    )
else()
  if (BUILD_SHARED_LIBRARY)
    add_library(OrthancFramework SHARED
      ${AUTOGENERATED_SOURCES}
      ${ORTHANC_CORE_SOURCES}
      ${ORTHANC_DICOM_SOURCES}
      DllMain.cpp
      )

    # By default, hide all the symbols
    set_target_properties(OrthancFramework PROPERTIES C_VISIBILITY_PRESET hidden)
    set_target_properties(OrthancFramework PROPERTIES CXX_VISIBILITY_PRESET hidden)

    # Configure the version of the shared library
    set_target_properties(
      OrthancFramework PROPERTIES 
      VERSION ${ORTHANC_VERSION} 
      SOVERSION ${ORTHANC_FRAMEWORK_SOVERSION}
      )    

    target_link_libraries(OrthancFramework ${DCMTK_LIBRARIES})

    if (LIBICU_LIBRARIES)
      target_link_libraries(OrthancFramework ${LIBICU_LIBRARIES})
    endif()

    if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
      target_link_libraries(OrthancFramework winpthread)
    endif()
  else()
    # Building a static library
    add_library(OrthancFramework STATIC
      ${AUTOGENERATED_SOURCES}
      ${ORTHANC_CORE_SOURCES}
      ${ORTHANC_DICOM_SOURCES}
      )

    # Add the "-fPIC" option to use the static library from Orthanc
    # plugins (the latter being shared libraries)
    set_property(TARGET OrthancFramework PROPERTY POSITION_INDEPENDENT_CODE ON)
  endif()
endif()



#####################################################################
## Publish the headers into the "Include" folder of the build
## directory
#####################################################################

file(
  COPY ${CMAKE_SOURCE_DIR}/../Sources/
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/orthanc-framework
  NO_SOURCE_PERMISSIONS
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN OrthancFramework.h EXCLUDE
  )

configure_file(
  ${CMAKE_SOURCE_DIR}/OrthancFramework.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/Include/orthanc-framework/OrthancFramework.h
  )


if (ORTHANC_STATIC_BOOST)
  file(
    COPY ${BOOST_SOURCES_DIR}/boost/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/boost/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "*.ipp"
    )
endif()


if (ENABLE_SQLITE AND ORTHANC_STATIC_SQLITE)
  file(
    COPY ${SQLITE_SOURCES_DIR}/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.h"
    )
endif()


if (ORTHANC_STATIC_JSONCPP)
  file(
    COPY ${JSONCPP_SOURCES_DIR}/include/json/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/json/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.h"
    )
endif()


if (ENABLE_DCMTK AND (STATIC_BUILD OR NOT USE_SYSTEM_DCMTK))
  file(
    COPY ${DCMTK_SOURCES_DIR}/dcmdata/include/dcmtk/dcmdata/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/dcmtk/dcmdata/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.h"
    )

  file(
    COPY ${DCMTK_SOURCES_DIR}/config/include/dcmtk/config/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/dcmtk/config/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.h"
    )

  file(
    COPY ${DCMTK_SOURCES_DIR}/ofstd/include/dcmtk/ofstd/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/dcmtk/ofstd/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.h"
    )

  file(
    COPY ${DCMTK_SOURCES_DIR}/oflog/include/dcmtk/oflog/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/dcmtk/oflog/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.h"
    )
endif()


if (ENABLE_PUGIXML AND (STATIC_BUILD OR NOT USE_SYSTEM_PUGIXML))
  file(
    COPY ${PUGIXML_SOURCES_DIR}/src/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.hpp"
    )
endif()


if (ENABLE_LUA AND (STATIC_BUILD OR NOT USE_SYSTEM_LUA))
  file(
    COPY ${LUA_SOURCES_DIR}/src/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/
    NO_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*.h"
    )
endif()


if (OFF)
  # These files are fully abstracted by the Orthanc Framework classes
  if (ENABLE_PNG AND (STATIC_BUILD OR NOT USE_SYSTEM_LIBPNG))
    file(
      COPY ${LIBPNG_SOURCES_DIR}/
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/
      NO_SOURCE_PERMISSIONS
      FILES_MATCHING
      PATTERN "*.h"
      )
  endif()

  if (ENABLE_ZLIB AND (STATIC_BUILD OR NOT USE_SYSTEM_ZLIB))
    file(
      COPY ${ZLIB_SOURCES_DIR}/
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/
      NO_SOURCE_PERMISSIONS
      FILES_MATCHING
      PATTERN "*.h"
      )
  endif()

  if (ENABLE_JPEG AND (STATIC_BUILD OR NOT USE_SYSTEM_LIBJPEG))
    file(
      COPY ${LIBJPEG_SOURCES_DIR}/
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/
      NO_SOURCE_PERMISSIONS
      FILES_MATCHING
      PATTERN "*.h"
      )
  endif()

  if (ENABLE_WEB_CLIENT AND (STATIC_BUILD OR NOT USE_SYSTEM_CURL))
    file(
      COPY ${CURL_SOURCES_DIR}/include/curl/
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Include/curl/
      NO_SOURCE_PERMISSIONS
      FILES_MATCHING
      PATTERN "*.h"
      )
  endif()
endif()



#####################################################################
## Possibly install the headers and the binaries
#####################################################################

install(
  TARGETS OrthancFramework
  RUNTIME DESTINATION ${ORTHANC_INSTALL_PREFIX}/lib    # Destination for Windows
  LIBRARY DESTINATION ${ORTHANC_INSTALL_PREFIX}/lib    # Destination for Linux
  ARCHIVE DESTINATION ${ORTHANC_INSTALL_PREFIX}/lib    # Destination for static library
  )

if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/OrthancFramework.wasm
    DESTINATION ${ORTHANC_INSTALL_PREFIX}/lib
    )
endif()

install(
  DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Include/
  DESTINATION ${ORTHANC_INSTALL_PREFIX}/include/
  )



#####################################################################
## Compile the unit tests
#####################################################################

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  include(ExternalProject)

  if (CMAKE_TOOLCHAIN_FILE)
    # Take absolute path to the toolchain
    get_filename_component(TMP ${CMAKE_TOOLCHAIN_FILE} REALPATH BASE ${CMAKE_SOURCE_DIR}/..)
    list(APPEND Flags
      -DCMAKE_TOOLCHAIN_FILE=${TMP}
      -DLSB_CC=${LSB_CC}
      -DLSB_CXX=${LSB_CXX}
      )
  endif()

  if (JSONCPP_CXX11)
    list(APPEND Flags -DUSE_CXX11=1)
  endif()
  
  # Build the unit tests, linking them against the just-created
  # "OrthancFramework" library
  externalproject_add(UnitTests
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/../UnitTestsSources"
    CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DORTHANC_FRAMEWORK_LIBDIR:PATH=${CMAKE_CURRENT_BINARY_DIR}
    -DORTHANC_FRAMEWORK_ROOT:PATH=${CMAKE_CURRENT_BINARY_DIR}/Include/orthanc-framework
    -DORTHANC_FRAMEWORK_SOURCE:STRING=system
    -DORTHANC_FRAMEWORK_STATIC:BOOL=${STATIC_BUILD}

    -DALLOW_DOWNLOADS:BOOL=${ALLOW_DOWNLOADS}
    -DBOOST_LOCALE_BACKEND:STRING=${BOOST_LOCALE_BACKEND}
    -DSTATIC_BUILD:BOOL=${STATIC_BUILD}
    -DUNIT_TESTS_WITH_HTTP_CONNEXIONS:BOOL=${UNIT_TESTS_WITH_HTTP_CONNEXIONS}
    -DUSE_GOOGLE_TEST_DEBIAN_PACKAGE:BOOL=${USE_GOOGLE_TEST_DEBIAN_PACKAGE}
    -DUSE_SYSTEM_BOOST:BOOL=${USE_SYSTEM_BOOST}
    -DUSE_SYSTEM_GOOGLE_TEST:BOOL=${USE_SYSTEM_GOOGLE_TEST}
    -DORTHANC_FRAMEWORK_USE_SHARED:BOOL=${BUILD_SHARED_LIBRARY}
    -DORTHANC_FRAMEWORK_ADDITIONAL_LIBRARIES:STRING=${ORTHANC_FRAMEWORK_ADDITIONAL_LIBRARIES}

    ${Flags}
    )

  add_dependencies(UnitTests OrthancFramework)
endif()



#####################################################################
## Prepare the "uninstall" target
## http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F
#####################################################################

configure_file(
  "${CMAKE_SOURCE_DIR}/../Resources/CMake/Uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
