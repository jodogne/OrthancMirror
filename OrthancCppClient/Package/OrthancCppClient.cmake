# nm -C -D --defined-only libOrthancCppClient.so 

include_directories(${ORTHANC_ROOT}/OrthancCppClient/Package/Laaw)

set(STATIC_BUILD ON)
include(${ORTHANC_ROOT}/Resources/CMake/DownloadPackage.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/JsonCppConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/LibCurlConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/LibPngConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/BoostConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/ZlibConfiguration.cmake)

if (${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wno-implicit-function-declaration")  # --std=c99 makes libcurl not to compile
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wno-long-long -Wno-variadic-macros")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
elseif (${MSVC})
  add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)  
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    set(WINDOWS_DEF ${ORTHANC_ROOT}/OrthancCppClient/Package/Build/Windows32.def)
  elseif (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(WINDOWS_DEF ${ORTHANC_ROOT}/OrthancCppClient/Package/Build/Windows64.def)
  else()
    message(FATAL_ERROR "Support your platform here")
  endif()
endif()

add_library(OrthancCppClient SHARED
  ${THIRD_PARTY_SOURCES}
  ${ORTHANC_ROOT}/Core/OrthancException.cpp
  ${ORTHANC_ROOT}/Core/Enumerations.cpp
  ${ORTHANC_ROOT}/Core/Toolbox.cpp
  ${ORTHANC_ROOT}/Core/HttpClient.cpp
  ${ORTHANC_ROOT}/Core/MultiThreading/ArrayFilledByThreads.cpp
  ${ORTHANC_ROOT}/Core/MultiThreading/ThreadedCommandProcessor.cpp
  ${ORTHANC_ROOT}/Core/MultiThreading/SharedMessageQueue.cpp
  ${ORTHANC_ROOT}/Core/FileFormats/PngReader.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/OrthancConnection.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Series.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Study.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Instance.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Patient.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Package/SharedLibrary.cpp
  ${ORTHANC_ROOT}/Resources/sha1/sha1.cpp
  ${ORTHANC_ROOT}/Resources/md5/md5.c
  ${ORTHANC_ROOT}/Resources/base64/base64.cpp
  ${WINDOWS_DEF}
  )

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script=${ORTHANC_ROOT}/OrthancCppClient/Package/Laaw/VersionScript.map")
  target_link_libraries(OrthancCppClient pthread)
else()
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--allow-multiple-definition -static-libgcc -static-libstdc++")
  target_link_libraries(OrthancCppClient ws2_32)
endif()
