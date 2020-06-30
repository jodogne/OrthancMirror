Generating ICU data file
========================

This folder generates the "icudtXXX_dat.c" file that contains the
resources internal to ICU.

IMPORTANT: Since ICU 59, C++11 is mandatory, making it incompatible
with Linux Standard Base (LSB) SDK. The option
"-DUSE_LEGACY_LIBICU=ON" will use the latest version of ICU that does
not use C++11 (58-2).


Usage
-----

Newest release of icu:

$ cmake .. -G Ninja && ninja install

Legacy version suitable for LSB:

$ cmake .. -G Ninja -DUSE_LEGACY_LIBICU=ON && ninja install

Legacy version, compiled using LSB:

$ LSB_CC=gcc-4.8 LSB_CXX=g++-4.8 cmake .. -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=../../../LinuxStandardBaseToolchain.cmake \
  -DUSE_LEGACY_LIBICU=ON
$ ninja install


Result
------

The resulting files are placed in the "ThirdPartyDownloads" folder at
the root of the Orthanc repository (next to the main "CMakeLists.txt").
