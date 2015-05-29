cmake_minimum_required(VERSION 2.8)

project(Basic)

set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../../)
include(${CMAKE_SOURCE_DIR}/../Common/OrthancPlugins.cmake)

add_library(PluginTest SHARED Plugin.cpp)
