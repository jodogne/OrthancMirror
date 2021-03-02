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


if (STATIC_BUILD OR NOT USE_SYSTEM_ZLIB)
  SET(ZLIB_SOURCES_DIR ${CMAKE_BINARY_DIR}/zlib-1.2.11)
  SET(ZLIB_URL "http://orthanc.osimis.io/ThirdPartyDownloads/zlib-1.2.11.tar.gz")
  SET(ZLIB_MD5 "1c9f62f0778697a09d36121ead88e08e")

  DownloadPackage(${ZLIB_MD5} ${ZLIB_URL} "${ZLIB_SOURCES_DIR}")

  include_directories(
    ${ZLIB_SOURCES_DIR}
    )

  list(APPEND ZLIB_SOURCES 
    ${ZLIB_SOURCES_DIR}/adler32.c
    ${ZLIB_SOURCES_DIR}/compress.c
    ${ZLIB_SOURCES_DIR}/crc32.c 
    ${ZLIB_SOURCES_DIR}/deflate.c 
    ${ZLIB_SOURCES_DIR}/infback.c 
    ${ZLIB_SOURCES_DIR}/inffast.c 
    ${ZLIB_SOURCES_DIR}/inflate.c 
    ${ZLIB_SOURCES_DIR}/inftrees.c 
    ${ZLIB_SOURCES_DIR}/trees.c 
    ${ZLIB_SOURCES_DIR}/uncompr.c 
    ${ZLIB_SOURCES_DIR}/zutil.c
    )

  if (NOT ORTHANC_SANDBOXED)
    # The source files below require access to the filesystem
    list(APPEND ZLIB_SOURCES
      ${ZLIB_SOURCES_DIR}/gzlib.c 
      ${ZLIB_SOURCES_DIR}/gzclose.c 
      ${ZLIB_SOURCES_DIR}/gzread.c 
      ${ZLIB_SOURCES_DIR}/gzwrite.c 
      )
  endif()

  source_group(ThirdParty\\zlib REGULAR_EXPRESSION ${ZLIB_SOURCES_DIR}/.*)

  if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
    # "ioapi.c" from zlib (minizip) expects the "IOAPI_NO_64" macro to be set to "true"
    # https://ohse.de/uwe/articles/lfs.html
    add_definitions(
      -DIOAPI_NO_64=1
      )
  endif()

else()
  include(FindZLIB)
  include_directories(${ZLIB_INCLUDE_DIRS})
  link_libraries(${ZLIB_LIBRARIES})
endif()
