cmake_minimum_required(VERSION 2.8)

project(ServeFolders)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  link_libraries(uuid)
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pthread")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  link_libraries(rpcrt4 ws2_32 secur32)
  if (CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
  endif()
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script=${CMAKE_SOURCE_DIR}/VersionScript.map -Wl,--no-undefined")
endif()

include_directories(${CMAKE_SOURCE_DIR}/../../Include/)
add_library(PluginTest SHARED Plugin.cpp)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Linking with "pthread" is necessary, otherwise the software crashes
  # http://sourceware.org/bugzilla/show_bug.cgi?id=10652#c17
  target_link_libraries(PluginTest pthread dl)
endif()
