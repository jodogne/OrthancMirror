if (APPLE)
  # Under OS X, the binaries must always be linked against the
  # system-wide version of SQLite. Otherwise, if some Orthanc plugin
  # also uses its own version of SQLite (such as orthanc-webviewer),
  # this results in a crash in "sqlite3_mutex_enter(db->mutex);" (the
  # mutex is not initialized), probably because the EXE and the DYNLIB
  # share the same memory location for this mutex.
  set(SQLITE_STATIC OFF)

elseif (STATIC_BUILD OR NOT USE_SYSTEM_SQLITE)
  set(SQLITE_STATIC ON)
else()
  set(SQLITE_STATIC OFF)
endif()


if (SQLITE_STATIC)
  SET(SQLITE_SOURCES_DIR ${CMAKE_BINARY_DIR}/sqlite-amalgamation-3210000)
  SET(SQLITE_MD5 "fe330e88d81e77e1e61554a370ae5001")
  SET(SQLITE_URL "http://www.orthanc-server.com/downloads/third-party/sqlite-amalgamation-3210000.zip")

  add_definitions(-DORTHANC_SQLITE_VERSION=3021000)

  DownloadPackage(${SQLITE_MD5} ${SQLITE_URL} "${SQLITE_SOURCES_DIR}")

  set(SQLITE_SOURCES
    ${SQLITE_SOURCES_DIR}/sqlite3.c
    )

  add_definitions(
    # For SQLite to run in the "Serialized" thread-safe mode
    # http://www.sqlite.org/threadsafe.html
    -DSQLITE_THREADSAFE=1  
    -DSQLITE_OMIT_LOAD_EXTENSION  # Disable SQLite plugins
    )

  include_directories(
    ${SQLITE_SOURCES_DIR}
    )

  source_group(ThirdParty\\SQLite REGULAR_EXPRESSION ${SQLITE_SOURCES_DIR}/.*)

else()
  CHECK_INCLUDE_FILE_CXX(sqlite3.h HAVE_SQLITE_H)
  if (NOT HAVE_SQLITE_H)
    message(FATAL_ERROR "Please install the libsqlite3-dev package")
  endif()

  find_path(SQLITE_INCLUDE_DIR sqlite3.h
    /usr/include
    /usr/local/include
    )
  message("SQLite include dir: ${SQLITE_INCLUDE_DIR}")

  # Autodetection of the version of SQLite
  file(STRINGS "${SQLITE_INCLUDE_DIR}/sqlite3.h" SQLITE_VERSION_NUMBER1 REGEX "#define SQLITE_VERSION_NUMBER.*$")    
  string(REGEX REPLACE "#define SQLITE_VERSION_NUMBER(.*)$" "\\1" SQLITE_VERSION_NUMBER2 ${SQLITE_VERSION_NUMBER1})

  # Remove the trailing spaces to convert the string to a proper integer
  string(STRIP ${SQLITE_VERSION_NUMBER2} SQLITE_VERSION_NUMBER)

  message("Detected version of SQLite: ${SQLITE_VERSION_NUMBER}")

  IF (${SQLITE_VERSION_NUMBER} LESS 3007000)
    # "sqlite3_create_function_v2" is not defined in SQLite < 3.7.0
    message(FATAL_ERROR "SQLite version must be above 3.7.0. Please set the CMake variable USE_SYSTEM_SQLITE to OFF.")
  ENDIF()

  add_definitions(-DORTHANC_SQLITE_VERSION=${SQLITE_VERSION_NUMBER})

  link_libraries(sqlite3)
endif()
