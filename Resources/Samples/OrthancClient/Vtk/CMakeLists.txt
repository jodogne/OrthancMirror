cmake_minimum_required(VERSION 2.8)

project(Vtk)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

add_executable(Test
  main.cpp
  )

# Linking with "pthread" is necessary, otherwise the software crashes
# http://sourceware.org/bugzilla/show_bug.cgi?id=10652#c17
target_link_libraries(Test pthread dl)

if(VTK_LIBRARIES)
  target_link_libraries(Test ${VTK_LIBRARIES})
else()
  target_link_libraries(Test vtkHybrid vtkVolumeRendering)
endif()
