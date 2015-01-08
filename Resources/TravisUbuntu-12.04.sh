#!/bin/sh

set -e

mkdir Build
cd Build

cmake \
    "-DDCMTK_LIBRARIES=CharLS;dcmjpls;wrap;oflog" \
    -DALLOW_DOWNLOADS=ON \
    -DUSE_SYSTEM_BOOST=OFF \
    -DUSE_SYSTEM_MONGOOSE=OFF \
    -DUSE_SYSTEM_JSONCPP=OFF \
    -DUSE_SYSTEM_GOOGLE_LOG=OFF \
    -DUSE_SYSTEM_PUGIXML=OFF \
    -DUSE_GTEST_DEBIAN_SOURCE_PACKAGE=ON \
    ..

make
