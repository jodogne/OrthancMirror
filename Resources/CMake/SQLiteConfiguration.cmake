if (STATIC_BUILD OR NOT USE_SYSTEM_SQLITE)
  SET(SQLITE_SOURCES_DIR ${CMAKE_BINARY_DIR}/sqlite-amalgamation-3071300)
  DownloadPackage(
    "5fbeff9645ab035a1f580e90b279a16d"
    "http://www.montefiore.ulg.ac.be/~jodogne/Orthanc/ThirdPartyDownloads/sqlite-amalgamation-3071300.zip"
    "${SQLITE_SOURCES_DIR}")

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
  string(REGEX REPLACE "#define SQLITE_VERSION_NUMBER(.*)$" "\\1" SQLITE_VERSION_NUMBER ${SQLITE_VERSION_NUMBER1})

  message("Detected version of SQLite: ${SQLITE_VERSION_NUMBER}")

  IF (${SQLITE_VERSION_NUMBER} LESS 3007000)
    # "sqlite3_create_function_v2" is not defined in SQLite < 3.7.0
    message(FATAL_ERROR "SQLite version must be above 3.7.0. Please set the CMake variable USE_SYSTEM_SQLITE to OFF.")
  ENDIF()

  link_libraries(sqlite3)
endif()
