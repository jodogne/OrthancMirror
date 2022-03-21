# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2022 Osimis S.A., Belgium
# Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program. If not, see
# <http://www.gnu.org/licenses/>.


# https://github.com/emscripten-core/emscripten/blob/master/src/settings.js

if (NOT "${EMSCRIPTEN_TRAP_MODE}" STREQUAL "")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s BINARYEN_TRAP_MODE='\"${EMSCRIPTEN_TRAP_MODE}\"'")
endif()

# If "-O3" is used (the default in "Release" mode), this results in a
# too large memory consumption in "wasm-opt", at least in Emscripten
# 3.1.7, which ultimately crashes the compiler. So we force "-O2"
# (this also has the advantage of speeding up the build):
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# "DISABLE_EXCEPTION_CATCHING" is a "compile+link" option. HOWEVER,
# setting it inside "WASM_FLAGS" creates link errors, at least with
# side modules. TODO: Understand why
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")

# "-Wno-unused-command-line-argument" is used to avoid annoying
# warnings about setting WASM, FETCH and ASSERTIONS, which was
# required for earlier versions of emsdk:
# https://groups.google.com/g/emscripten-discuss/c/VX4enWfadUE
set(WASM_FLAGS "${WASM_FLAGS} -Wno-unused-command-line-argument")

#set(WASM_FLAGS "${WASM_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")

if (EMSCRIPTEN_TARGET_MODE STREQUAL "wasm")
  # WebAssembly
  set(WASM_FLAGS "${WASM_FLAGS} -s WASM=1")
  
elseif (EMSCRIPTEN_TARGET_MODE STREQUAL "asm.js")
  # asm.js targeting IE 11
  set(WASM_FLAGS "-s WASM=0 -s ASM_JS=2 -s LEGACY_VM_SUPPORT=1")

else()
  message(FATAL_ERROR "Bad value for EMSCRIPTEN_TARGET_MODE: ${EMSCRIPTEN_TARGET_MODE}")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(WASM_FLAGS "${WASM_FLAGS} -s SAFE_HEAP=1 -s ASSERTIONS=1")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WASM_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WASM_FLAGS}")
