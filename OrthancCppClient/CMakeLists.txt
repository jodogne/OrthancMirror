# Mini-project to check whether "OrthancCppClient" can compile in a
# standalone fashion

cmake_minimum_required(VERSION 2.8)

project(OrthancCppClientTest)

SET(STATIC_BUILD OFF)

include(${CMAKE_SOURCE_DIR}/../Resources/CMake/DownloadPackage.cmake)
include(${CMAKE_SOURCE_DIR}/../Resources/CMake/JsonCppConfiguration.cmake)
include(${CMAKE_SOURCE_DIR}/../Resources/CMake/LibCurlConfiguration.cmake)

if (${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_C_FLAGS "-Wall -pedantic -Wno-implicit-function-declaration")  # --std=c99 makes libcurl not to compile
  set(CMAKE_CXX_FLAGS "-Wall -pedantic -Wno-long-long -Wno-variadic-macros")
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed")
  set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
elseif (${MSVC})
  add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)  
endif()

add_library(OrthancCppClient
  SHARED

  ${THIRD_PARTY_SOURCES}
  HttpException.cpp
  HttpClient.cpp
  )

add_executable(Test
  main.cpp
  )

target_link_libraries(Test OrthancCppClient)
