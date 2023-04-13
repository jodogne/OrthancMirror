# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

  if (STATIC_BUILD OR NOT USE_SYSTEM_UUID)
    SET(E2FSPROGS_SOURCES_DIR ${CMAKE_BINARY_DIR}/e2fsprogs-1.44.5)
    SET(E2FSPROGS_URL "https://orthanc.uclouvain.be/third-party-downloads/e2fsprogs-1.44.5.tar.gz")
    SET(E2FSPROGS_MD5 "8d78b11d04d26c0b2dd149529441fa80")

    if (IS_DIRECTORY "${E2FSPROGS_SOURCES_DIR}")
      set(FirstRun OFF)
    else()
      set(FirstRun ON)
    endif()

    DownloadPackage(${E2FSPROGS_MD5} ${E2FSPROGS_URL} "${E2FSPROGS_SOURCES_DIR}")

    
    ##
    ## Patch for OS X, in order to be compatible with Cocoa, and for
    ## WebAssembly (used in Stone)
    ## 

    execute_process(
      COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
      ${CMAKE_CURRENT_LIST_DIR}/../Patches/e2fsprogs-1.44.5.patch
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE Failure
      )

    if (FirstRun AND Failure)
      message(FATAL_ERROR "Error while patching a file")
    endif()


    include_directories(
      BEFORE ${E2FSPROGS_SOURCES_DIR}/lib
      )

    set(UUID_SOURCES
      #${E2FSPROGS_SOURCES_DIR}/lib/uuid/tst_uuid.c
      #${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid_time.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/clear.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/compare.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/copy.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/gen_uuid.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/isnull.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/pack.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/parse.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/unpack.c
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/unparse.c
      )

    check_include_file("net/if.h"       HAVE_NET_IF_H)
    check_include_file("net/if_dl.h"    HAVE_NET_IF_DL_H)
    check_include_file("netinet/in.h"   HAVE_NETINET_IN_H)
    check_include_file("stdlib.h"       HAVE_STDLIB_H)
    check_include_file("sys/file.h"     HAVE_SYS_FILE_H)
    check_include_file("sys/ioctl.h"    HAVE_SYS_IOCTL_H)
    check_include_file("sys/resource.h" HAVE_SYS_RESOURCE_H)
    check_include_file("sys/socket.h"   HAVE_SYS_SOCKET_H)
    check_include_file("sys/sockio.h"   HAVE_SYS_SOCKIO_H)
    check_include_file("sys/syscall.h"  HAVE_SYS_SYSCALL_H)
    check_include_file("sys/time.h"     HAVE_SYS_TIME_H)
    check_include_file("sys/un.h"       HAVE_SYS_UN_H)
    check_include_file("unistd.h"       HAVE_UNISTD_H)

    if (NOT HAVE_NET_IF_H)  # This is the case of OpenBSD
      unset(HAVE_NET_IF_H CACHE)
      check_include_files("sys/socket.h;net/if.h" HAVE_NET_IF_H)
    endif()

    if (NOT HAVE_NETINET_TCP_H)  # This is the case of OpenBSD
      unset(HAVE_NETINET_TCP_H CACHE)
      check_include_files("sys/socket.h;netinet/tcp.h" HAVE_NETINET_TCP_H)
    endif()

    if (NOT EXISTS ${E2FSPROGS_SOURCES_DIR}/lib/uuid/config.h)
      file(WRITE ${E2FSPROGS_SOURCES_DIR}/lib/uuid/config.h.cmake "
#cmakedefine HAVE_NET_IF_H \@HAVE_NET_IF_H\@
#cmakedefine HAVE_NET_IF_DL_H \@HAVE_NET_IF_DL_H\@
#cmakedefine HAVE_NETINET_IN_H \@HAVE_NETINET_IN_H\@
#cmakedefine HAVE_STDLIB_H \@HAVE_STDLIB_H \@
#cmakedefine HAVE_SYS_FILE_H \@HAVE_SYS_FILE_H\@
#cmakedefine HAVE_SYS_IOCTL_H \@HAVE_SYS_IOCTL_H\@
#cmakedefine HAVE_SYS_RESOURCE_H \@HAVE_SYS_RESOURCE_H\@
#cmakedefine HAVE_SYS_SOCKET_H \@HAVE_SYS_SOCKET_H\@
#cmakedefine HAVE_SYS_SOCKIO_H \@HAVE_SYS_SOCKIO_H\@
#cmakedefine HAVE_SYS_SYSCALL_H \@HAVE_SYS_SYSCALL_H\@
#cmakedefine HAVE_SYS_TIME_H \@HAVE_SYS_TIME_H\@
#cmakedefine HAVE_SYS_UN_H \@HAVE_SYS_UN_H\@
#cmakedefine HAVE_UNISTD_H \@HAVE_UNISTD_H\@
")
    endif()
      
    configure_file(
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/config.h.cmake
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/config.h
      )
      
    configure_file(
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid.h.in
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid.h
      )

    if (NOT EXISTS ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid_types.h)
      file(WRITE
        ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid_types.h
        "#include <stdint.h>\n")
    endif()
    
    source_group(ThirdParty\\uuid REGULAR_EXPRESSION ${E2FSPROGS_SOURCES_DIR}/.*)

  else()
    CHECK_INCLUDE_FILE(uuid/uuid.h HAVE_UUID_H)
    if (NOT HAVE_UUID_H)
      message(FATAL_ERROR "Please install uuid-dev, e2fsprogs (OpenBSD) or e2fsprogs-libuuid (FreeBSD)")
    endif()

    find_library(LIBUUID uuid
      PATHS
      /usr/lib
      /usr/local/lib
      )

    check_library_exists(${LIBUUID} uuid_generate_random "" HAVE_LIBUUID)
    if (NOT HAVE_LIBUUID)
      message(FATAL_ERROR "Unable to find the uuid library")
    endif()
    
    link_libraries(${LIBUUID})
  endif()

endif()
