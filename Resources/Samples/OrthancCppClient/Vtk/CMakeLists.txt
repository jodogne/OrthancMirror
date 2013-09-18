cmake_minimum_required(VERSION 2.8)

project(OrthancCppClientTest)

set(STATIC_BUILD OFF)
set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../../..)

include(../OrthancCppClient.cmake)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_executable(Test
  main.cpp
  ${ORTHANC_CPP_CLIENT_SOURCES}
  )

if(VTK_LIBRARIES)
  target_link_libraries(Test ${VTK_LIBRARIES})
else()
  target_link_libraries(Test vtkHybrid vtkVolumeRendering)
endif()

