if (STATIC_BUILD OR NOT USE_SYSTEM_LUA)
  SET(LUA_SOURCES_DIR ${CMAKE_BINARY_DIR}/lua-5.1.5)
  SET(LUA_MD5 "2e115fe26e435e33b0d5c022e4490567")
  SET(LUA_URL "http://www.orthanc-server.com/downloads/third-party/lua-5.1.5.tar.gz")

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

    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
      add_definitions(-DLUA_DL_DLL=1)       # Enable loading of shared libraries (for Microsoft Windows)
      
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
      add_definitions(-LUA_DL_DYLD=1)       # Enable loading of shared libraries (for Apple OS X)
      
    else()
      message(FATAL_ERROR "Support your platform here")
    endif()
  endif()

  include_directories(
    ${LUA_SOURCES_DIR}/src
    )

  set(LUA_SOURCES
    # Core Lua
    ${LUA_SOURCES_DIR}/src/lapi.c
    ${LUA_SOURCES_DIR}/src/lcode.c 
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
    ${LUA_SOURCES_DIR}/src/ldblib.c
    ${LUA_SOURCES_DIR}/src/liolib.c
    ${LUA_SOURCES_DIR}/src/lmathlib.c
    ${LUA_SOURCES_DIR}/src/loslib.c
    ${LUA_SOURCES_DIR}/src/ltablib.c
    ${LUA_SOURCES_DIR}/src/lstrlib.c
    ${LUA_SOURCES_DIR}/src/loadlib.c
    ${LUA_SOURCES_DIR}/src/linit.c
    )

  source_group(ThirdParty\\Lua REGULAR_EXPRESSION ${LUA_SOURCES_DIR}/.*)

else()
  include(FindLua51)

  if (NOT LUA51_FOUND)
    message(FATAL_ERROR "Please install the liblua-dev package")
  endif()

  include_directories(${LUA_INCLUDE_DIR})
  link_libraries(${LUA_LIBRARIES})
endif()
