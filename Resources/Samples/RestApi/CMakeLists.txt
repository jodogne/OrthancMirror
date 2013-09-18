cmake_minimum_required(VERSION 2.8)

project(RestApiSample)

include(ExternalProject)

# Send the toolchain information to the Orthanc 
if (CMAKE_TOOLCHAIN_FILE)
  set(TOOLCHAIN "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
endif()

ExternalProject_Add(
  ORTHANC_CORE

  # We use the Orthanc-0.3.1 branch for this sample
  DOWNLOAD_COMMAND hg clone https://code.google.com/p/orthanc/ -r Orthanc-0.3.1

  # Optional step, to reuse the third-party downloads
  PATCH_COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/../../../ThirdPartyDownloads ThirdPartyDownloads

  PREFIX ${CMAKE_BINARY_DIR}/Orthanc/
  UPDATE_COMMAND ""
  SOURCE_DIR ${CMAKE_BINARY_DIR}/Orthanc/src/orthanc/
  CMAKE_COMMAND ${CMAKE_COMMAND}
  CMAKE_ARGS -DSTATIC_BUILD=ON -DSTANDALONE_BUILD=ON -DUSE_DYNAMIC_GOOGLE_LOG=OFF -DUSE_DYNAMIC_SQLITE=OFF -DENABLE_SSL=OFF ${TOOLCHAIN}
  BUILD_COMMAND $(MAKE) CoreLibrary
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE 0
  )

ExternalProject_Get_Property(ORTHANC_CORE source_dir)
include_directories(${source_dir})

ExternalProject_Get_Property(ORTHANC_CORE binary_dir)
link_directories(${binary_dir})
include_directories(${binary_dir}/jsoncpp-src-0.5.0/include)
include_directories(${binary_dir}/glog-0.3.2/src)
include_directories(${binary_dir}/boost_1_49_0)


add_executable(RestApiSample
  Sample.cpp
  )

add_dependencies(RestApiSample ORTHANC_CORE)

target_link_libraries(RestApiSample 
  # From Orthanc
  CoreLibrary
  GoogleLog

  # These two libraries are not necessary
  #OpenSSL
  #Curl
  )

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  target_link_libraries(RestApiSample pthread)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  add_definitions(-DGOOGLE_GLOG_DLL_DECL=)
  target_link_libraries(RestApiSample wsock32)
endif()
