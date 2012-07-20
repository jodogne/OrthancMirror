SET(SQLITE_SOURCES_DIR ${CMAKE_BINARY_DIR}/sqlite-amalgamation-3071300)
DownloadPackage("http://www.sqlite.org/sqlite-amalgamation-3071300.zip" "${SQLITE_SOURCES_DIR}" "" "")

list(APPEND THIRD_PARTY_SOURCES
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