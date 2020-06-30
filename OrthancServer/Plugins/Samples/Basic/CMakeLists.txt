cmake_minimum_required(VERSION 2.8)

project(Basic)

include(${CMAKE_SOURCE_DIR}/../Common/OrthancPlugins.cmake)

add_library(PluginTest SHARED Plugin.c)
