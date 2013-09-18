# Mini-project to check whether "OrthancCppClient" can compile in a
# standalone fashion

cmake_minimum_required(VERSION 2.8)

project(OrthancCppClientTest)

set(STATIC_BUILD OFF)
set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../../..)

include(../OrthancCppClient.cmake)

add_executable(Test
  main.cpp
  ${ORTHANC_CPP_CLIENT_SOURCES}
  )
