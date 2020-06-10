cmake_minimum_required(VERSION 2.8)

project(Basic)

set(SAMPLES_ROOT ${CMAKE_SOURCE_DIR}/..)
include(${SAMPLES_ROOT}/Common/OrthancPlugins.cmake)

add_library(PluginTest SHARED Plugin.c)
