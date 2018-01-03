if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

  if (STATIC_BUILD OR NOT USE_SYSTEM_UUID)
    SET(E2FSPROGS_SOURCES_DIR ${CMAKE_BINARY_DIR}/e2fsprogs-1.43.8)
    SET(E2FSPROGS_URL "http://www.orthanc-server.com/downloads/third-party/e2fsprogs-1.43.8.tar.gz")
    SET(E2FSPROGS_MD5 "670b7a74a8ead5333acf21b9afc92b3c")

    DownloadPackage(${E2FSPROGS_MD5} ${E2FSPROGS_URL} "${E2FSPROGS_SOURCES_DIR}")

    include_directories(
      ${E2FSPROGS_SOURCES_DIR}/lib
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
    
    configure_file(
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/config.h.cmake
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/config.h
      )
    
    
    configure_file(
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid.h.in
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid.h
      )

    file(WRITE
      ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid_types.h
      "#include <stdint.h>\n")

    #configure_file(
    #  ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid_types.h.in
    #  ${E2FSPROGS_SOURCES_DIR}/lib/uuid/uuid_types.h
    #  )
    
    source_group(ThirdParty\\uuid REGULAR_EXPRESSION ${E2FSPROGS_SOURCES_DIR}/.*)

  else()
    CHECK_INCLUDE_FILE(uuid/uuid.h HAVE_UUID_H)
    if (NOT HAVE_UUID_H)
      message(FATAL_ERROR "Please install uuid-dev, e2fsprogs (OpenBSD) or e2fsprogs-libuuid (FreeBSD)")
    endif()

    check_library_exists(uuid uuid_generate_random "" HAVE_UUID_LIB)
    if (NOT HAVE_UUID_LIB)
      message(FATAL_ERROR "Unable to find the uuid library")
    endif()
    
    link_libraries(uuid)
  endif()

endif()
