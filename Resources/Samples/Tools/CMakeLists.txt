cmake_minimum_required(VERSION 2.8)

project(OrthancTools)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Linking with "pthread" is necessary, otherwise the software crashes
  # http://sourceware.org/bugzilla/show_bug.cgi?id=10652#c17
  link_libraries(pthread dl)
endif()

include(${CMAKE_SOURCE_DIR}/../../CMake/OrthancFrameworkParameters.cmake)

set(STATIC_BUILD ON)
set(ALLOW_DOWNLOADS ON)

include(${CMAKE_SOURCE_DIR}/../../CMake/OrthancFrameworkConfiguration.cmake)

add_library(CommonLibraries
  ${BOOST_SOURCES}
  ${JSONCPP_SOURCES}
  ${ORTHANC_ROOT}/Core/Enumerations.cpp
  ${ORTHANC_ROOT}/Core/Logging.cpp
  ${ORTHANC_ROOT}/Core/SystemToolbox.cpp
  ${ORTHANC_ROOT}/Core/Toolbox.cpp
  ${ORTHANC_ROOT}/Resources/ThirdParty/md5/md5.c
  ${ORTHANC_ROOT}/Resources/ThirdParty/base64/base64.cpp
  )

add_executable(RecoverCompressedFile
  RecoverCompressedFile.cpp
  ${ORTHANC_ROOT}/Core/Compression/DeflateBaseCompressor.cpp
  ${ORTHANC_ROOT}/Core/Compression/ZlibCompressor.cpp
  ${ZLIB_SOURCES}
  )

target_link_libraries(RecoverCompressedFile CommonLibraries)
