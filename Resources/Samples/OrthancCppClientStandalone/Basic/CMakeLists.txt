cmake_minimum_required(VERSION 2.8)

project(Basic)

add_executable(Test main.cpp)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Linking with "pthread" is necessary, otherwise the software crashes
  # http://sourceware.org/bugzilla/show_bug.cgi?id=10652#c17
  target_link_libraries(Test pthread dl)
endif()
