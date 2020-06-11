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
