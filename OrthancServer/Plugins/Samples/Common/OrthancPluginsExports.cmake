# In Orthanc <= 1.7.1, the instructions below were part of
# "Compiler.cmake", and were protected by the (now unused) option
# "ENABLE_PLUGINS_VERSION_SCRIPT" in CMake

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/VersionScriptPlugins.map")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -exported_symbols_list ${CMAKE_CURRENT_LIST_DIR}/ExportedSymbolsPlugins.list")
endif()
