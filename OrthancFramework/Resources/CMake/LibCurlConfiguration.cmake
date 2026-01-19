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


if (STATIC_BUILD OR NOT USE_SYSTEM_CURL)
  SET(CURL_SOURCES_DIR ${CMAKE_BINARY_DIR}/curl-8.17.0)
  SET(CURL_URL "https://orthanc.uclouvain.be/downloads/third-party-downloads/curl-8.17.0.tar.gz")
  SET(CURL_MD5 "71e24b00f40a7503c1d07886e42d6305")

  if (IS_DIRECTORY "${CURL_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()
  
  DownloadPackage(${CURL_MD5} ${CURL_URL} "${CURL_SOURCES_DIR}")

  if (FirstRun)
    execute_process(
      COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
      ${CMAKE_CURRENT_LIST_DIR}/../Patches/curl-8.17.0.patch
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE Failure
      )
    
    if (Failure)
      message(FATAL_ERROR "Error while patching a file")
    endif()
  endif()
  
  include_directories(
    ${CURL_SOURCES_DIR}/include
    )

  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib CURL_SOURCES)
  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib/vauth CURL_SOURCES)
  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib/vssh CURL_SOURCES)
  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib/vtls CURL_SOURCES)
  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib/vquic CURL_SOURCES)
  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib/curlx CURL_SOURCES)
  source_group(ThirdParty\\LibCurl REGULAR_EXPRESSION ${CURL_SOURCES_DIR}/.*)

  add_definitions(
    -DBUILDING_LIBCURL=1
    -DCURL_STATICLIB=1
    -DCURL_DISABLE_LDAPS=1
    -DCURL_DISABLE_LDAP=1
    -DCURL_DISABLE_DICT=1
    -DCURL_DISABLE_FILE=1
    -DCURL_DISABLE_FTP=1
    -DCURL_DISABLE_GOPHER=1
    -DCURL_DISABLE_LDAP=1
    -DCURL_DISABLE_LDAPS=1
    -DCURL_DISABLE_POP3=1
    #-DCURL_DISABLE_PROXY=1
    -DCURL_DISABLE_RTSP=1
    -DCURL_DISABLE_TELNET=1
    -DCURL_DISABLE_TFTP=1
    -DUSE_IPV6=1   # new in 1.12.11
    )

  if (ENABLE_SSL)
    add_definitions(
      #-DHAVE_LIBSSL=1
      -DUSE_OPENSSL=1
      -DHAVE_OPENSSL_ENGINE_H=1
      -DUSE_SSLEAY=1
      )
  endif()

  if (NOT EXISTS "${CURL_SOURCES_DIR}/lib/vauth/vauth/vauth.h")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/vauth/digest.h "#include \"../digest.h\"\n")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/vauth/ntlm.h "#include \"../ntlm.h\"\n")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/vauth/vauth.h "#include \"../vauth.h\"\n")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/vtls/vtls.h "#include \"../../vtls/vtls.h\"\n")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vssh/curl_setup.h "#include \"../curl_setup.h\"\n")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vtls/vauth/vauth.h "#include \"../../vauth/vauth.h\"\n")

    file(GLOB CURL_LIBS_HEADERS ${CURL_SOURCES_DIR}/lib/*.h)
    foreach (header IN LISTS CURL_LIBS_HEADERS)
      get_filename_component(filename ${header} NAME)
      file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/${filename} "#include \"../${filename}\"\n")
      file(WRITE ${CURL_SOURCES_DIR}/lib/vquic/${filename} "#include \"../${filename}\"\n")
      file(WRITE ${CURL_SOURCES_DIR}/lib/vtls/${filename} "#include \"../${filename}\"\n")
    endforeach()
  endif()

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
    if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      SET(TMP_OS "x86_64")
    else()
      SET(TMP_OS "x86")
    endif()

    # _GNU_SOURCE is required for accept4(), pipe2(), sendmmsg()
    set_property(
      SOURCE ${CURL_SOURCES} APPEND
      PROPERTY COMPILE_DEFINITIONS "HAVE_CONFIG_H=1;OS=\"${TMP_OS}\";_GNU_SOURCE"
      )

    if(CMAKE_C_COMPILER_TARGET)
      set(CURL_OS "\"${CMAKE_C_COMPILER_TARGET}\"")
    else()
      set(CURL_OS "\"${CMAKE_SYSTEM_NAME}\"")
    endif()

    include(${CURL_SOURCES_DIR}/CMake/Macros.cmake)

    # Detect headers

    # Use check_include_file_concat_curl() for headers required by subsequent
    # check_include_file_concat_curl() or check_symbol_exists() detections.
    # Order for these is significant.
    check_include_file("sys/eventfd.h"    HAVE_SYS_EVENTFD_H)
    check_include_file("sys/filio.h"      HAVE_SYS_FILIO_H)
    check_include_file("sys/ioctl.h"      HAVE_SYS_IOCTL_H)
    check_include_file("sys/param.h"      HAVE_SYS_PARAM_H)
    check_include_file("sys/poll.h"       HAVE_SYS_POLL_H)
    check_include_file("sys/resource.h"   HAVE_SYS_RESOURCE_H)
    check_include_file_concat_curl("sys/select.h"     HAVE_SYS_SELECT_H)
    check_include_file("sys/sockio.h"     HAVE_SYS_SOCKIO_H)
    check_include_file_concat_curl("sys/types.h"      HAVE_SYS_TYPES_H)
    check_include_file("sys/un.h"         HAVE_SYS_UN_H)
    check_include_file_concat_curl("sys/utime.h"      HAVE_SYS_UTIME_H)  # sys/types.h (AmigaOS)

    check_include_file_concat_curl("arpa/inet.h"      HAVE_ARPA_INET_H)
    check_include_file("dirent.h"         HAVE_DIRENT_H)
    check_include_file("fcntl.h"          HAVE_FCNTL_H)
    check_include_file_concat_curl("ifaddrs.h"        HAVE_IFADDRS_H)
    check_include_file("io.h"             HAVE_IO_H)
    check_include_file_concat_curl("libgen.h"         HAVE_LIBGEN_H)
    check_include_file("linux/tcp.h"      HAVE_LINUX_TCP_H)
    check_include_file("locale.h"         HAVE_LOCALE_H)
    check_include_file_concat_curl("net/if.h"         HAVE_NET_IF_H)  # sys/select.h (e.g. MS-DOS/Watt-32)
    check_include_file_concat_curl("netdb.h"          HAVE_NETDB_H)
    check_include_file_concat_curl("netinet/in.h"     HAVE_NETINET_IN_H)
    check_include_file("netinet/in6.h"    HAVE_NETINET_IN6_H)
    check_include_file_concat_curl("netinet/tcp.h"    HAVE_NETINET_TCP_H)  # sys/types.h (e.g. Cygwin) netinet/in.h
    check_include_file_concat_curl("netinet/udp.h"    HAVE_NETINET_UDP_H)  # sys/types.h (e.g. Cygwin)
    check_include_file("poll.h"           HAVE_POLL_H)
    check_include_file("pwd.h"            HAVE_PWD_H)
    check_include_file("stdatomic.h"      HAVE_STDATOMIC_H)
    check_include_file("stdbool.h"        HAVE_STDBOOL_H)
    check_include_file("stdint.h"         HAVE_STDINT_H)
    check_include_file("strings.h"        HAVE_STRINGS_H)
    check_include_file("stropts.h"        HAVE_STROPTS_H)
    check_include_file("termio.h"         HAVE_TERMIO_H)
    check_include_file("termios.h"        HAVE_TERMIOS_H)
    check_include_file_concat_curl("unistd.h"         HAVE_UNISTD_H)
    check_include_file("utime.h"          HAVE_UTIME_H)

    check_type_size("size_t"  SIZEOF_SIZE_T)
    check_type_size("ssize_t"  SIZEOF_SSIZE_T)
    check_type_size("long long"  SIZEOF_LONG_LONG)
    check_type_size("long"  SIZEOF_LONG)
    check_type_size("short"  SIZEOF_SHORT)
    check_type_size("int"  SIZEOF_INT)
    check_type_size("__int64"  SIZEOF___INT64)
    check_type_size("long double"  SIZEOF_LONG_DOUBLE)
    check_type_size("time_t"  SIZEOF_TIME_T)
    check_type_size("off_t"  SIZEOF_OFF_T)
    check_type_size("socklen_t" CURL_SIZEOF_CURL_SOCKLEN_T)

    if (SIZEOF_LONG_LONG)
      set(HAVE_LONGLONG 1)
    endif()

    check_function_exists("accept4"       HAVE_ACCEPT4)
    check_function_exists("fnmatch"       HAVE_FNMATCH)
    check_symbol_exists("basename"        "${CURL_INCLUDES};string.h" HAVE_BASENAME)  # libgen.h unistd.h
    check_symbol_exists("opendir"         "dirent.h" HAVE_OPENDIR)
    check_function_exists("poll"          HAVE_POLL)  # poll.h
    check_symbol_exists("socket"          "${CURL_INCLUDES}" HAVE_SOCKET)  # winsock2.h sys/socket.h
    check_symbol_exists("socketpair"      "${CURL_INCLUDES}" HAVE_SOCKETPAIR)  # sys/socket.h
    check_symbol_exists("recv"            "${CURL_INCLUDES}" HAVE_RECV)  # proto/bsdsocket.h sys/types.h sys/socket.h
    check_symbol_exists("send"            "${CURL_INCLUDES}" HAVE_SEND)  # proto/bsdsocket.h sys/types.h sys/socket.h
    check_function_exists("sendmsg"       HAVE_SENDMSG)
    check_function_exists("sendmmsg"      HAVE_SENDMMSG)
    check_symbol_exists("select"          "${CURL_INCLUDES}" HAVE_SELECT)  # proto/bsdsocket.h sys/select.h sys/socket.h
    check_symbol_exists("strdup"          "string.h" HAVE_STRDUP)
    check_symbol_exists("memrchr"         "string.h" HAVE_MEMRCHR)
    check_symbol_exists("alarm"           "unistd.h" HAVE_ALARM)
    check_symbol_exists("fcntl"           "fcntl.h" HAVE_FCNTL)
    check_function_exists("getppid"       HAVE_GETPPID)
    check_function_exists("utimes"        HAVE_UTIMES)

    check_function_exists("gettimeofday"  HAVE_GETTIMEOFDAY)  # sys/time.h
    check_symbol_exists("closesocket"     "${CURL_INCLUDES}" HAVE_CLOSESOCKET)  # winsock2.h
    check_symbol_exists("sigsetjmp"       "setjmp.h" HAVE_SIGSETJMP)
    check_function_exists("getpass_r"     HAVE_GETPASS_R)
    check_function_exists("getpwuid"      HAVE_GETPWUID)
    check_function_exists("getpwuid_r"    HAVE_GETPWUID_R)
    check_function_exists("geteuid"       HAVE_GETEUID)
    check_function_exists("utime"         HAVE_UTIME)
    check_symbol_exists("gmtime_r"        "stdlib.h;time.h" HAVE_GMTIME_R)

    check_symbol_exists("gethostbyname_r" "netdb.h" HAVE_GETHOSTBYNAME_R)
    check_symbol_exists("gethostname"     "${CURL_INCLUDES}" HAVE_GETHOSTNAME)  # winsock2.h unistd.h proto/bsdsocket.h

    check_symbol_exists("signal"          "signal.h" HAVE_SIGNAL)
    check_symbol_exists("strerror_r"      "stdlib.h;string.h" HAVE_STRERROR_R)
    check_symbol_exists("sigaction"       "signal.h" HAVE_SIGACTION)
    check_symbol_exists("siginterrupt"    "signal.h" HAVE_SIGINTERRUPT)
    check_symbol_exists("getaddrinfo"     "${CURL_INCLUDES};stdlib.h;string.h" HAVE_GETADDRINFO)  # ws2tcpip.h sys/socket.h netdb.h
    check_symbol_exists("getifaddrs"      "${CURL_INCLUDES};stdlib.h" HAVE_GETIFADDRS)  # ifaddrs.h
    check_symbol_exists("freeaddrinfo"    "${CURL_INCLUDES}" HAVE_FREEADDRINFO)  # ws2tcpip.h sys/socket.h netdb.h
    check_function_exists("pipe"          HAVE_PIPE)
    check_function_exists("pipe2"         HAVE_PIPE2)
    check_function_exists("eventfd"       HAVE_EVENTFD)
    check_symbol_exists("ftruncate"       "unistd.h" HAVE_FTRUNCATE)
    check_symbol_exists("getpeername"     "${CURL_INCLUDES}" HAVE_GETPEERNAME)  # winsock2.h unistd.h proto/bsdsocket.h
    check_symbol_exists("getsockname"     "${CURL_INCLUDES}" HAVE_GETSOCKNAME)  # winsock2.h unistd.h proto/bsdsocket.h
    check_function_exists("getrlimit"       HAVE_GETRLIMIT)
    check_function_exists("setlocale"       HAVE_SETLOCALE)
    check_function_exists("setrlimit"       HAVE_SETRLIMIT)

    if(WIN32)
      # include wincrypt.h as a workaround for mingw-w64 __MINGW64_VERSION_MAJOR <= 5 header bug */
      check_symbol_exists("if_nametoindex"  "winsock2.h;wincrypt.h;iphlpapi.h" HAVE_IF_NAMETOINDEX)  # Windows Vista+ non-UWP */
    else()
      check_function_exists("if_nametoindex"  HAVE_IF_NAMETOINDEX)  # net/if.h
      check_function_exists("realpath"        HAVE_REALPATH)
      check_function_exists("sched_yield"     HAVE_SCHED_YIELD)
      check_symbol_exists("strcasecmp"      "string.h" HAVE_STRCASECMP)
      check_symbol_exists("stricmp"         "string.h" HAVE_STRICMP)
      check_symbol_exists("strcmpi"         "string.h" HAVE_STRCMPI)
    endif()
    check_struct_has_member("struct sockaddr_un" sun_path "sys/un.h" USE_UNIX_SOCKETS)

    list(APPEND CMAKE_REQUIRED_INCLUDES "${CURL_SOURCES_DIR}/include")
    set(CMAKE_EXTRA_INCLUDE_FILES "curl/system.h")
    check_type_size("curl_off_t"  SIZEOF_CURL_OFF_T)

    include(${CURL_SOURCES_DIR}/CMake/OtherTests.cmake)

    foreach(CURL_TEST
        HAVE_FCNTL_O_NONBLOCK
        HAVE_IOCTLSOCKET
        HAVE_IOCTLSOCKET_CAMEL
        HAVE_IOCTLSOCKET_CAMEL_FIONBIO
        HAVE_IOCTLSOCKET_FIONBIO
        HAVE_IOCTL_FIONBIO
        HAVE_IOCTL_SIOCGIFADDR
        HAVE_SETSOCKOPT_SO_NONBLOCK
        HAVE_GETHOSTBYNAME_R_3
        HAVE_GETHOSTBYNAME_R_5
        HAVE_GETHOSTBYNAME_R_6
        HAVE_BOOL_T
        STDC_HEADERS
        HAVE_ATOMIC
        HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID
        HAVE_GETHOSTBYNAME_R_3_REENTRANT
        HAVE_GETHOSTBYNAME_R_5_REENTRANT
        HAVE_GETHOSTBYNAME_R_6_REENTRANT
        HAVE_GETADDRINFO
        HAVE_FILE_OFFSET_BITS
        HAVE_GLIBC_STRERROR_R
        HAVE_POSIX_STRERROR_R
        )
      curl_internal_test(${CURL_TEST})
    endforeach(CURL_TEST)

    configure_file(
      ${CURL_SOURCES_DIR}/lib/curl_config.h.cmake
      ${CURL_SOURCES_DIR}/lib/curl_config.h
      )
  endif()

elseif (CMAKE_CROSSCOMPILING AND
    "${CMAKE_SYSTEM_VERSION}" STREQUAL "CrossToolNg")

  CHECK_INCLUDE_FILE_CXX(curl/curl.h HAVE_CURL_H)
  if (NOT HAVE_CURL_H)
    message(FATAL_ERROR "Please install the libcurl-dev package")
  endif()

  CHECK_LIBRARY_EXISTS(curl "curl_easy_init" "" HAVE_CURL_LIB)
  if (NOT HAVE_CURL_LIB)
    message(FATAL_ERROR "Please install the libcurl package")
  endif()  
  
  link_libraries(curl)

else()
  include(FindCURL)
  include_directories(${CURL_INCLUDE_DIRS})
  link_libraries(${CURL_LIBRARIES})

  if (NOT ${CURL_FOUND})
    message(FATAL_ERROR "Unable to find LibCurl")
  endif()
endif()
