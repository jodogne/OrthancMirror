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


if (STATIC_BUILD OR NOT USE_SYSTEM_LIBPNG)
  SET(LIBPNG_SOURCES_DIR ${CMAKE_BINARY_DIR}/libpng-1.6.36)
  SET(LIBPNG_URL "http://orthanc.osimis.io/ThirdPartyDownloads/libpng-1.6.36.tar.gz")
  SET(LIBPNG_MD5 "65afdeaa05f5ec14e31d9276143012e9")

  DownloadPackage(${LIBPNG_MD5} ${LIBPNG_URL} "${LIBPNG_SOURCES_DIR}")

  include_directories(
    ${LIBPNG_SOURCES_DIR}
    )

  configure_file(
    ${LIBPNG_SOURCES_DIR}/scripts/pnglibconf.h.prebuilt
    ${LIBPNG_SOURCES_DIR}/pnglibconf.h
    )

  set(LIBPNG_SOURCES
    #${LIBPNG_SOURCES_DIR}/example.c
    ${LIBPNG_SOURCES_DIR}/png.c
    ${LIBPNG_SOURCES_DIR}/pngerror.c
    ${LIBPNG_SOURCES_DIR}/pngget.c
    ${LIBPNG_SOURCES_DIR}/pngmem.c
    ${LIBPNG_SOURCES_DIR}/pngpread.c
    ${LIBPNG_SOURCES_DIR}/pngread.c
    ${LIBPNG_SOURCES_DIR}/pngrio.c
    ${LIBPNG_SOURCES_DIR}/pngrtran.c
    ${LIBPNG_SOURCES_DIR}/pngrutil.c
    ${LIBPNG_SOURCES_DIR}/pngset.c
    #${LIBPNG_SOURCES_DIR}/pngtest.c
    ${LIBPNG_SOURCES_DIR}/pngtrans.c
    ${LIBPNG_SOURCES_DIR}/pngwio.c
    ${LIBPNG_SOURCES_DIR}/pngwrite.c
    ${LIBPNG_SOURCES_DIR}/pngwtran.c
    ${LIBPNG_SOURCES_DIR}/pngwutil.c
    )

  add_definitions(
    -DPNG_NO_CONFIG_H=1
    -DPNG_NO_CONSOLE_IO=1
    -DPNG_NO_STDIO=1
    # The following declaration avoids "__declspec(dllexport)" in
    # libpng to prevent publicly exposing its symbols by the DLLs
    -DPNG_IMPEXP=
    )

  source_group(ThirdParty\\libpng REGULAR_EXPRESSION ${LIBPNG_SOURCES_DIR}/.*)

else()
  include(FindPNG)

  if (NOT ${PNG_FOUND})
    message(FATAL_ERROR "Unable to find libpng")
  endif()

  include_directories(${PNG_INCLUDE_DIRS})
  link_libraries(${PNG_LIBRARIES})
  add_definitions(${PNG_DEFINITIONS})
endif()
