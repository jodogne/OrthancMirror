if (STATIC_BUILD OR NOT USE_SYSTEM_LUA)
  SET(LUA_SOURCES_DIR ${CMAKE_BINARY_DIR}/lua-5.1.5)
  DownloadPackage(
    "2e115fe26e435e33b0d5c022e4490567"
    "http://www.orthanc-server.com/downloads/third-party/lua-5.1.5.tar.gz"
    "${LUA_SOURCES_DIR}")

  add_definitions(
    #-DLUA_LIB=1
    #-Dluaall_c=1
    #-DLUA_COMPAT_ALL=1  # Compile a generic version of Lua
    )

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

  add_library(Lua STATIC ${LUA_SOURCES})
  set(STATIC_LUA Lua)

  source_group(ThirdParty\\Lua REGULAR_EXPRESSION ${LUA_SOURCES_DIR}/.*)

else()
  include(FindLua51)

  if (NOT LUA51_FOUND)
    message(FATAL_ERROR "Please install the liblua-dev package")
  endif()

  include_directories(${LUA_INCLUDE_DIR})
  link_libraries(${LUA_LIBRARIES})
endif()
