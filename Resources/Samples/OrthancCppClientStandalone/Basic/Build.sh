#!/bin/bash

rm -rf i w32 w64
mkdir i
mkdir w32
mkdir w64

cd i && cmake .. && cd .. && \
    cd w32 && cmake .. -DCMAKE_TOOLCHAIN_FILE=../../../../Resources/MinGWToolchain.cmake && cd .. && \
    cd w64 && cmake .. -DCMAKE_TOOLCHAIN_FILE=../../../../Resources/MinGW64Toolchain.cmake && cd ..

make -C i -j12
make -C w32 -j12
make -C w64 -j12

# nm -C -D --defined-only i/libOrthancCppClient.so 
