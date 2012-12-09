cmake_minimum_required(VERSION 2.8)

project(RestApiSample)

include(ExternalProject)

ExternalProject_Add(
  ORTHANC_CORE
  PREFIX ${CMAKE_BINARY_DIR}/Orthanc/
  DOWNLOAD_COMMAND hg clone https://code.google.com/p/orthanc/ -r Orthanc-0.3.1
  UPDATE_COMMAND ""
  SOURCE_DIR ${CMAKE_BINARY_DIR}/Orthanc/src/orthanc/

  # Optional step, to reuse the third-party downloads
  PATCH_COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/../../../ThirdPartyDownloads ThirdPartyDownloads

  CMAKE_COMMAND ${CMAKE_COMMAND}
  CMAKE_ARGS -DSTATIC_BUILD=ON -DSTANDALONE_BUILD=ON -DUSE_DYNAMIC_GOOGLE_LOG=OFF -DUSE_DYNAMIC_SQLITE=OFF -DONLY_CORE_LIBRARY=ON -DENABLE_SSL=OFF
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE 0
)

ExternalProject_Get_Property(ORTHANC_CORE source_dir)
include_directories(${source_dir})

ExternalProject_Get_Property(ORTHANC_CORE binary_dir)
link_directories(${binary_dir})
include_directories(${binary_dir}/jsoncpp-src-0.5.0/include)
include_directories(${binary_dir}/glog-0.3.2/src)

add_executable(RestApiSample
  Sample.cpp
  )

add_dependencies(RestApiSample ORTHANC_CORE)

target_link_libraries(RestApiSample 
  # From Orthanc
  CoreLibrary
  GoogleLog
  #OpenSSL

  # System-wide libraries
  pthread 
  )

