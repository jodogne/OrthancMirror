cmake_minimum_required(VERSION 2.8)

project(Basic)

if (${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Werror")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Werror")
endif()

include_directories(${CMAKE_SOURCE_DIR}/../../OrthancCPlugin/)
add_library(PluginTest SHARED Plugin.c)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Linking with "pthread" is necessary, otherwise the software crashes
  # http://sourceware.org/bugzilla/show_bug.cgi?id=10652#c17
  target_link_libraries(PluginTest pthread dl)
endif()
