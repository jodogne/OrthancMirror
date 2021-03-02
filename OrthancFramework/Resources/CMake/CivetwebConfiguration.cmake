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


if (STATIC_BUILD OR NOT USE_SYSTEM_CIVETWEB)

  ## WARNING: "civetweb-1.13.tar.gz" comes with a subfolder
  ## "civetweb-1.13/test/nonlatin" that cannot be removed by "hg purge
  ## --all" on Windows hosts. We thus created a custom
  ## "civetweb-1.13-fixed.tar.gz" as follows:
  ##
  ##  $ cd /tmp
  ##  $ wget http://orthanc.osimis.io/ThirdPartyDownloads/civetweb-1.13.tar.gz
  ##  $ tar xvf civetweb-1.13.tar.gz
  ##  $ rm -rf civetweb-1.13/src/third_party/ civetweb-1.13/test/
  ##  $ tar cvfz civetweb-1.13-fixed.tar.gz civetweb-1.13
  ##
  
  set(CIVETWEB_SOURCES_DIR ${CMAKE_BINARY_DIR}/civetweb-1.13)
  set(CIVETWEB_URL "http://orthanc.osimis.io/ThirdPartyDownloads/civetweb-1.13-fixed.tar.gz")
  set(CIVETWEB_MD5 "9cf9d22cf8a06a8487d98637bbcd543c")

  if (IS_DIRECTORY "${CIVETWEB_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()

  DownloadPackage(${CIVETWEB_MD5} ${CIVETWEB_URL} "${CIVETWEB_SOURCES_DIR}")

  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/civetweb-1.13.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (FirstRun AND Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()
  
  include_directories(
    ${CIVETWEB_SOURCES_DIR}/include
    )

  set(CIVETWEB_SOURCES
    ${CIVETWEB_SOURCES_DIR}/src/civetweb.c
    )

  # New in Orthanc 1.6.0: Enable support of compression in civetweb
  set(tmp "USE_ZLIB=1")
    
  if (ENABLE_SSL)
    add_definitions(
      -DNO_SSL_DL=1
      )
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
        ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
      link_libraries(dl)
    endif()

    if (CIVETWEB_OPENSSL_API STREQUAL "1.0")
      set(tmp "${tmp};OPENSSL_API_1_0=1")
    elseif (CIVETWEB_OPENSSL_API STREQUAL "1.1")
      set(tmp "${tmp};OPENSSL_API_1_1=1")
    else()
      message(FATAL_ERROR "Unsupported value for CIVETWEB_OPENSSL_API: ${CIVETWEB_OPENSSL_API}")
    endif()

  else()
    add_definitions(
      -DNO_SSL=1   # Remove SSL support from civetweb
      )
  endif()

  set_source_files_properties(
    ${CIVETWEB_SOURCES}
    PROPERTIES COMPILE_DEFINITIONS "${tmp}"
    )
  
  source_group(ThirdParty\\Civetweb REGULAR_EXPRESSION ${CIVETWEB_SOURCES_DIR}/.*)

  add_definitions(
    -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=1
    -DCIVETWEB_HAS_WEBDAV_WRITING=1
    )

else()
  CHECK_INCLUDE_FILE_CXX(civetweb.h HAVE_CIVETWEB_H)
  if (NOT HAVE_CIVETWEB_H)
    message(FATAL_ERROR "Please install the libcivetweb-dev package")
  endif()

  cmake_reset_check_state()
  set(CMAKE_REQUIRED_LIBRARIES dl pthread)
  CHECK_LIBRARY_EXISTS(civetweb mg_start "" HAVE_CIVETWEB_LIB)
  if (NOT HAVE_CIVETWEB_LIB)
    message(FATAL_ERROR "Please install the libcivetweb-dev package")
  endif()

  link_libraries(civetweb)
  unset(CMAKE_REQUIRED_LIBRARIES)

  # Check whether the system distribution of civetweb contains the
  # patch "../Patches/civetweb-1.13.patch" that allows to disable
  # keep-alive on selected HTTP connections. This is useful to speed
  # up multipart transfers, as encountered in DICOMweb.
  CHECK_LIBRARY_EXISTS(civetweb mg_disable_keep_alive "" CIVETWEB_HAS_DISABLE_KEEP_ALIVE)
  if (CIVETWEB_HAS_DISABLE_KEEP_ALIVE)
    add_definitions(
      -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=1
      -DCIVETWEB_HAS_WEBDAV_WRITING=1
      )
    message("Performance: Your system-wide distribution of civetweb is configured for best performance")
  else()
    message(WARNING "Performance: Your system-wide distribution of civetweb does not feature the mg_disable_keep_alive() function, and WebDAV will only be available for read-only access")
    add_definitions(
      -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=0
      -DCIVETWEB_HAS_WEBDAV_WRITING=0
      )
  endif()
endif()


# New in Orthanc 1.8.1
add_definitions(
  -DCIVETWEB_KEEP_ALIVE_TIMEOUT_SECONDS=1
  )
