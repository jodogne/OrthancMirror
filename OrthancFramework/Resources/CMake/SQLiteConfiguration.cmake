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
  SET(SQLITE_SOURCES_DIR ${CMAKE_BINARY_DIR}/sqlite-amalgamation-3270100)
  SET(SQLITE_MD5 "16717b26358ba81f0bfdac07addc77da")
  SET(SQLITE_URL "http://orthanc.osimis.io/ThirdPartyDownloads/sqlite-amalgamation-3270100.zip")

  set(ORTHANC_SQLITE_VERSION 3027001)

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
  CHECK_INCLUDE_FILE(sqlite3.h HAVE_SQLITE_H)
  if (NOT HAVE_SQLITE_H)
    message(FATAL_ERROR "Please install the libsqlite3-dev package")
  endif()

  find_path(SQLITE_INCLUDE_DIR
    NAMES sqlite3.h
    PATHS
    /usr/include
    /usr/local/include
    )
  message("SQLite include dir: ${SQLITE_INCLUDE_DIR}")

  # Autodetection of the version of SQLite
  file(STRINGS "${SQLITE_INCLUDE_DIR}/sqlite3.h" SQLITE_VERSION_NUMBER1 REGEX "#define SQLITE_VERSION_NUMBER.*$")    
  string(REGEX REPLACE "#define SQLITE_VERSION_NUMBER(.*)$" "\\1" SQLITE_VERSION_NUMBER2 ${SQLITE_VERSION_NUMBER1})

  # Remove the trailing spaces to convert the string to a proper integer
  string(STRIP ${SQLITE_VERSION_NUMBER2} ORTHANC_SQLITE_VERSION)

  message("Detected version of SQLite: ${ORTHANC_SQLITE_VERSION}")

  IF (${ORTHANC_SQLITE_VERSION} LESS 3007000)
    # "sqlite3_create_function_v2" is not defined in SQLite < 3.7.0
    message(FATAL_ERROR "SQLite version must be above 3.7.0. Please set the CMake variable USE_SYSTEM_SQLITE to OFF.")
  ENDIF()

  link_libraries(sqlite3)
endif()


add_definitions(
  -DORTHANC_SQLITE_VERSION=${ORTHANC_SQLITE_VERSION}
  )
