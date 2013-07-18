cmake_minimum_required(VERSION 2.8)

project(OrthancCppClientTest)

set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../../..)
include(../../OrthancCppClient.cmake)

add_executable(Test main.cpp)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  target_link_libraries(Test dl)
else()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()
