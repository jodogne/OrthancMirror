# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
# Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


if (NOT ENABLE_ZLIB)
  message(FATAL_ERROR "This file cannot be used if zlib is disabled")
endif()

if (STATIC_BUILD OR
    ORTHANC_SANDBOXED OR   # For WebAssembly
    NOT USE_SYSTEM_MINIZIP)
  add_definitions(-DORTHANC_USE_SYSTEM_MINIZIP=0)

  list(APPEND ORTHANC_CORE_SOURCES_DEPENDENCIES
    ${CMAKE_CURRENT_LIST_DIR}/../../Resources/ThirdParty/minizip/ioapi.c
    ${CMAKE_CURRENT_LIST_DIR}/../../Resources/ThirdParty/minizip/unzip.c
    ${CMAKE_CURRENT_LIST_DIR}/../../Resources/ThirdParty/minizip/zip.c
    )

else()
  add_definitions(-DORTHANC_USE_SYSTEM_MINIZIP=1)

  CHECK_INCLUDE_FILE_CXX(minizip/zip.h HAVE_MINIZIP_H)
  if (NOT HAVE_MINIZIP_H)
    message(FATAL_ERROR "Please install the libminizip-dev package")
  endif()

  CHECK_LIBRARY_EXISTS(minizip "zipOpen2_64" "" HAVE_MINIZIP_LIB)
  if (NOT HAVE_MINIZIP_LIB)
    message(FATAL_ERROR "Please install the libminizip-dev package")
  endif()

  link_libraries(minizip)

endif()
