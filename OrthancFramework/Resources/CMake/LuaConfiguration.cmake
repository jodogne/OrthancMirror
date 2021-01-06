# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
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


if (STATIC_BUILD OR NOT USE_SYSTEM_LUA)
  SET(LUA_SOURCES_DIR ${CMAKE_BINARY_DIR}/lua-5.3.5)
  SET(LUA_MD5 "4f4b4f323fd3514a68e0ab3da8ce3455")
  SET(LUA_URL "http://orthanc.osimis.io/ThirdPartyDownloads/lua-5.3.5.tar.gz")

  DownloadPackage(${LUA_MD5} ${LUA_URL} "${LUA_SOURCES_DIR}")

  if (ENABLE_LUA_MODULES)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
        ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" OR
        ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
        ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
      # Enable loading of shared libraries (for UNIX-like)
      add_definitions(-DLUA_USE_DLOPEN=1)

      # Publish the functions of the Lua engine (that are built within
      # the Orthanc binary) as global symbols, so that the external
      # shared libraries can call them
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--export-dynamic")

      if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
          ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
        add_definitions(-DLUA_USE_LINUX=1)
      elseif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
        add_definitions(
          -DLUA_USE_LINUX=1
          -DLUA_USE_READLINE=1
          )
      elseif (${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
        add_definitions(-DLUA_USE_POSIX=1)
      endif()

    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
      add_definitions(
        -DLUA_DL_DLL=1       # Enable loading of shared libraries (for Microsoft Windows)
        )
      
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
      add_definitions(
        -DLUA_USE_MACOSX=1
        -DLUA_DL_DYLD=1       # Enable loading of shared libraries (for Apple OS X)
        )
      
    else()
      message(FATAL_ERROR "Support your platform here")
    endif()
  endif()

  add_definitions(
    -DLUA_COMPAT_5_2=1
    )

  include_directories(
    ${LUA_SOURCES_DIR}/src
    )

  set(LUA_SOURCES
    # Don't compile the Lua command-line
    #${LUA_SOURCES_DIR}/src/lua.c
    #${LUA_SOURCES_DIR}/src/luac.c

    # Core Lua
    ${LUA_SOURCES_DIR}/src/lapi.c
    ${LUA_SOURCES_DIR}/src/lcode.c
    ${LUA_SOURCES_DIR}/src/lctype.c
    ${LUA_SOURCES_DIR}/src/ldebug.c
    ${LUA_SOURCES_DIR}/src/ldo.c
    ${LUA_SOURCES_DIR}/src/ldump.c
    ${LUA_SOURCES_DIR}/src/lfunc.c
    ${LUA_SOURCES_DIR}/src/lgc.c
    ${LUA_SOURCES_DIR}/src/llex.c
    ${LUA_SOURCES_DIR}/src/lmem.c
    ${LUA_SOURCES_DIR}/src/lobject.c
    ${LUA_SOURCES_DIR}/src/lopcodes.c
    ${LUA_SOURCES_DIR}/src/lparser.c
    ${LUA_SOURCES_DIR}/src/lstate.c
    ${LUA_SOURCES_DIR}/src/lstring.c
    ${LUA_SOURCES_DIR}/src/ltable.c
    ${LUA_SOURCES_DIR}/src/ltm.c
    ${LUA_SOURCES_DIR}/src/lundump.c
    ${LUA_SOURCES_DIR}/src/lvm.c
    ${LUA_SOURCES_DIR}/src/lzio.c

    # Base Lua modules
    ${LUA_SOURCES_DIR}/src/lauxlib.c
    ${LUA_SOURCES_DIR}/src/lbaselib.c
    ${LUA_SOURCES_DIR}/src/lbitlib.c
    ${LUA_SOURCES_DIR}/src/lcorolib.c
    ${LUA_SOURCES_DIR}/src/ldblib.c
    ${LUA_SOURCES_DIR}/src/liolib.c
    ${LUA_SOURCES_DIR}/src/lmathlib.c
    ${LUA_SOURCES_DIR}/src/loadlib.c
    ${LUA_SOURCES_DIR}/src/loslib.c
    ${LUA_SOURCES_DIR}/src/lstrlib.c
    ${LUA_SOURCES_DIR}/src/ltablib.c
    ${LUA_SOURCES_DIR}/src/lutf8lib.c

    ${LUA_SOURCES_DIR}/src/linit.c
    )

  source_group(ThirdParty\\Lua REGULAR_EXPRESSION ${LUA_SOURCES_DIR}/.*)

elseif (CMAKE_CROSSCOMPILING AND
    "${CMAKE_SYSTEM_VERSION}" STREQUAL "CrossToolNg")

  set(LUA_VERSIONS 5.3 5.2 5.1)

  unset(LUA_VERSION)
  foreach(version IN ITEMS ${LUA_VERSIONS})
    CHECK_INCLUDE_FILE(lua${version}/lua.h HAVE_LUA${version}_H)
    if (HAVE_LUA${version}_H)
      set(LUA_VERSION ${version})
      break()
    endif()
  endforeach()

  if (NOT LUA_VERSION)
    message(FATAL_ERROR "Please install the liblua-dev package")
  endif()
  
  CHECK_LIBRARY_EXISTS(lua${LUA_VERSION} "lua_call" "${LUA_LIB_DIR}" HAVE_LUA_LIB)
  if (NOT HAVE_LUA_LIB)
    message(FATAL_ERROR "Please install the liblua package")
  endif()  

  include_directories(${CROSSTOOL_NG_IMAGE}/usr/include/lua${LUA_VERSION})
  link_libraries(lua${LUA_VERSION})

else()
  include(FindLua)

  if (NOT LUA_FOUND)
    message(FATAL_ERROR "Please install the liblua-dev package")
  endif()

  include_directories(${LUA_INCLUDE_DIR})
  link_libraries(${LUA_LIBRARIES})
endif()
