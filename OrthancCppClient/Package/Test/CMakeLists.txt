cmake_minimum_required(VERSION 2.8)

project(OrthancCppClientTest)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_executable(Test
  main.cpp
  )

if(VTK_LIBRARIES)
  target_link_libraries(Test ${VTK_LIBRARIES})
else()
  target_link_libraries(Test vtkHybrid vtkVolumeRendering)
endif()

set(CMAKE_CXX_FLAGS "-fpermissive")  #TODO REMOVE
