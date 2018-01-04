if (STATIC_BUILD OR NOT USE_SYSTEM_CURL)
  SET(CURL_SOURCES_DIR ${CMAKE_BINARY_DIR}/curl-7.57.0)
  SET(CURL_URL "http://www.orthanc-server.com/downloads/third-party/curl-7.57.0.tar.gz")
  SET(CURL_MD5 "c7aab73aaf5e883ca1d7518f93649dc2")

  if (IS_DIRECTORY "${CURL_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()
  
  DownloadPackage(${CURL_MD5} ${CURL_URL} "${CURL_SOURCES_DIR}")

  if (FirstRun)
    execute_process(
      COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
      ${ORTHANC_ROOT}/Resources/Patches/curl-7.57.0-cmake.patch
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
  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib/vtls CURL_SOURCES)
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
    )

  if (ENABLE_SSL)
    add_definitions(
      #-DHAVE_LIBSSL=1
      -DUSE_OPENSSL=1
      -DHAVE_OPENSSL_ENGINE_H=1
      -DUSE_SSLEAY=1
      )
  endif()

  if (NOT EXISTS "${CURL_SOURCES_DIR}/lib/curl_config.h")
    #file(WRITE ${CURL_SOURCES_DIR}/lib/curl_config.h "")

    file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/vauth/vauth.h "#include \"../vauth.h\"\n")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/vauth/digest.h "#include \"../digest.h\"\n")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/vauth/ntlm.h "#include \"../ntlm.h\"\n")
    file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/vtls/vtls.h "#include \"../../vtls/vtls.h\"\n")

    file(GLOB CURL_LIBS_HEADERS ${CURL_SOURCES_DIR}/lib/*.h)
    foreach (header IN LISTS CURL_LIBS_HEADERS)
      get_filename_component(filename ${header} NAME)
      file(WRITE ${CURL_SOURCES_DIR}/lib/vauth/${filename} "#include \"../${filename}\"\n")
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

    set_property(
      SOURCE ${CURL_SOURCES}
      PROPERTY COMPILE_DEFINITIONS "HAVE_CONFIG_H=1;OS=\"${TMP_OS}\""
      )
   
    include(${CURL_SOURCES_DIR}/CMake/Macros.cmake)

    # WARNING: Do *not* reorder the "check_include_file_concat()" below!
    check_include_file_concat("stdio.h"          HAVE_STDIO_H)
    check_include_file_concat("inttypes.h"       HAVE_INTTYPES_H)
    check_include_file_concat("sys/filio.h"      HAVE_SYS_FILIO_H)
    check_include_file_concat("sys/ioctl.h"      HAVE_SYS_IOCTL_H)
    check_include_file_concat("sys/param.h"      HAVE_SYS_PARAM_H)
    check_include_file_concat("sys/poll.h"       HAVE_SYS_POLL_H)
    check_include_file_concat("sys/resource.h"   HAVE_SYS_RESOURCE_H)
    check_include_file_concat("sys/select.h"     HAVE_SYS_SELECT_H)
    check_include_file_concat("sys/socket.h"     HAVE_SYS_SOCKET_H)
    check_include_file_concat("sys/sockio.h"     HAVE_SYS_SOCKIO_H)
    check_include_file_concat("sys/stat.h"       HAVE_SYS_STAT_H)
    check_include_file_concat("sys/time.h"       HAVE_SYS_TIME_H)
    check_include_file_concat("sys/types.h"      HAVE_SYS_TYPES_H)
    check_include_file_concat("sys/uio.h"        HAVE_SYS_UIO_H)
    check_include_file_concat("sys/un.h"         HAVE_SYS_UN_H)
    check_include_file_concat("sys/utime.h"      HAVE_SYS_UTIME_H)
    check_include_file_concat("sys/xattr.h"      HAVE_SYS_XATTR_H)
    check_include_file_concat("alloca.h"         HAVE_ALLOCA_H)
    check_include_file_concat("arpa/inet.h"      HAVE_ARPA_INET_H)
    check_include_file_concat("arpa/tftp.h"      HAVE_ARPA_TFTP_H)
    check_include_file_concat("assert.h"         HAVE_ASSERT_H)
    check_include_file_concat("crypto.h"         HAVE_CRYPTO_H)
    check_include_file_concat("des.h"            HAVE_DES_H)
    check_include_file_concat("err.h"            HAVE_ERR_H)
    check_include_file_concat("errno.h"          HAVE_ERRNO_H)
    check_include_file_concat("fcntl.h"          HAVE_FCNTL_H)
    check_include_file_concat("idn2.h"           HAVE_IDN2_H)
    check_include_file_concat("ifaddrs.h"        HAVE_IFADDRS_H)
    check_include_file_concat("io.h"             HAVE_IO_H)
    check_include_file_concat("krb.h"            HAVE_KRB_H)
    check_include_file_concat("libgen.h"         HAVE_LIBGEN_H)
    check_include_file_concat("limits.h"         HAVE_LIMITS_H)
    check_include_file_concat("locale.h"         HAVE_LOCALE_H)
    check_include_file_concat("net/if.h"         HAVE_NET_IF_H)
    check_include_file_concat("netdb.h"          HAVE_NETDB_H)
    check_include_file_concat("netinet/in.h"     HAVE_NETINET_IN_H)
    check_include_file_concat("netinet/tcp.h"    HAVE_NETINET_TCP_H)

    check_include_file_concat("pem.h"            HAVE_PEM_H)
    check_include_file_concat("poll.h"           HAVE_POLL_H)
    check_include_file_concat("pwd.h"            HAVE_PWD_H)
    check_include_file_concat("rsa.h"            HAVE_RSA_H)
    check_include_file_concat("setjmp.h"         HAVE_SETJMP_H)
    check_include_file_concat("sgtty.h"          HAVE_SGTTY_H)
    check_include_file_concat("signal.h"         HAVE_SIGNAL_H)
    check_include_file_concat("ssl.h"            HAVE_SSL_H)
    check_include_file_concat("stdbool.h"        HAVE_STDBOOL_H)
    check_include_file_concat("stdint.h"         HAVE_STDINT_H)
    check_include_file_concat("stdio.h"          HAVE_STDIO_H)
    check_include_file_concat("stdlib.h"         HAVE_STDLIB_H)
    check_include_file_concat("string.h"         HAVE_STRING_H)
    check_include_file_concat("strings.h"        HAVE_STRINGS_H)
    check_include_file_concat("stropts.h"        HAVE_STROPTS_H)
    check_include_file_concat("termio.h"         HAVE_TERMIO_H)
    check_include_file_concat("termios.h"        HAVE_TERMIOS_H)
    check_include_file_concat("time.h"           HAVE_TIME_H)
    check_include_file_concat("unistd.h"         HAVE_UNISTD_H)
    check_include_file_concat("utime.h"          HAVE_UTIME_H)
    check_include_file_concat("x509.h"           HAVE_X509_H)

    check_include_file_concat("process.h"        HAVE_PROCESS_H)
    check_include_file_concat("stddef.h"         HAVE_STDDEF_H)
    check_include_file_concat("dlfcn.h"          HAVE_DLFCN_H)
    check_include_file_concat("malloc.h"         HAVE_MALLOC_H)
    check_include_file_concat("memory.h"         HAVE_MEMORY_H)
    check_include_file_concat("netinet/if_ether.h" HAVE_NETINET_IF_ETHER_H)
    check_include_file_concat("stdint.h"        HAVE_STDINT_H)
    check_include_file_concat("sockio.h"        HAVE_SOCKIO_H)
    check_include_file_concat("sys/utsname.h"   HAVE_SYS_UTSNAME_H)

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

    check_symbol_exists(basename      "${CURL_INCLUDES}" HAVE_BASENAME)
    check_symbol_exists(socket        "${CURL_INCLUDES}" HAVE_SOCKET)
    # poll on macOS is unreliable, it first did not exist, then was broken until
    # fixed in 10.9 only to break again in 10.12.
    if(NOT APPLE)
      check_symbol_exists(poll        "${CURL_INCLUDES}" HAVE_POLL)
    endif()
    check_symbol_exists(select        "${CURL_INCLUDES}" HAVE_SELECT)
    check_symbol_exists(strdup        "${CURL_INCLUDES}" HAVE_STRDUP)
    check_symbol_exists(strstr        "${CURL_INCLUDES}" HAVE_STRSTR)
    check_symbol_exists(strtok_r      "${CURL_INCLUDES}" HAVE_STRTOK_R)
    check_symbol_exists(strftime      "${CURL_INCLUDES}" HAVE_STRFTIME)
    check_symbol_exists(uname         "${CURL_INCLUDES}" HAVE_UNAME)
    check_symbol_exists(strcasecmp    "${CURL_INCLUDES}" HAVE_STRCASECMP)
    check_symbol_exists(stricmp       "${CURL_INCLUDES}" HAVE_STRICMP)
    check_symbol_exists(strcmpi       "${CURL_INCLUDES}" HAVE_STRCMPI)
    check_symbol_exists(strncmpi      "${CURL_INCLUDES}" HAVE_STRNCMPI)
    check_symbol_exists(alarm         "${CURL_INCLUDES}" HAVE_ALARM)
    if(NOT HAVE_STRNCMPI)
      set(HAVE_STRCMPI)
    endif(NOT HAVE_STRNCMPI)

    check_symbol_exists(gethostbyaddr "${CURL_INCLUDES}" HAVE_GETHOSTBYADDR)
    check_symbol_exists(gethostbyaddr_r "${CURL_INCLUDES}" HAVE_GETHOSTBYADDR_R)
    check_symbol_exists(gettimeofday  "${CURL_INCLUDES}" HAVE_GETTIMEOFDAY)
    check_symbol_exists(inet_addr     "${CURL_INCLUDES}" HAVE_INET_ADDR)
    check_symbol_exists(inet_ntoa     "${CURL_INCLUDES}" HAVE_INET_NTOA)
    check_symbol_exists(inet_ntoa_r   "${CURL_INCLUDES}" HAVE_INET_NTOA_R)
    check_symbol_exists(tcsetattr     "${CURL_INCLUDES}" HAVE_TCSETATTR)
    check_symbol_exists(tcgetattr     "${CURL_INCLUDES}" HAVE_TCGETATTR)
    check_symbol_exists(perror        "${CURL_INCLUDES}" HAVE_PERROR)
    check_symbol_exists(closesocket   "${CURL_INCLUDES}" HAVE_CLOSESOCKET)
    check_symbol_exists(setvbuf       "${CURL_INCLUDES}" HAVE_SETVBUF)
    check_symbol_exists(sigsetjmp     "${CURL_INCLUDES}" HAVE_SIGSETJMP)
    check_symbol_exists(getpass_r     "${CURL_INCLUDES}" HAVE_GETPASS_R)
    check_symbol_exists(strlcat       "${CURL_INCLUDES}" HAVE_STRLCAT)
    check_symbol_exists(getpwuid      "${CURL_INCLUDES}" HAVE_GETPWUID)
    check_symbol_exists(geteuid       "${CURL_INCLUDES}" HAVE_GETEUID)
    check_symbol_exists(utime         "${CURL_INCLUDES}" HAVE_UTIME)
    check_symbol_exists(gmtime_r      "${CURL_INCLUDES}" HAVE_GMTIME_R)
    check_symbol_exists(localtime_r   "${CURL_INCLUDES}" HAVE_LOCALTIME_R)

    check_symbol_exists(gethostbyname   "${CURL_INCLUDES}" HAVE_GETHOSTBYNAME)
    check_symbol_exists(gethostbyname_r "${CURL_INCLUDES}" HAVE_GETHOSTBYNAME_R)

    check_symbol_exists(signal        "${CURL_INCLUDES}" HAVE_SIGNAL_FUNC)
    check_symbol_exists(SIGALRM       "${CURL_INCLUDES}" HAVE_SIGNAL_MACRO)
    if(HAVE_SIGNAL_FUNC AND HAVE_SIGNAL_MACRO)
      set(HAVE_SIGNAL 1)
    endif(HAVE_SIGNAL_FUNC AND HAVE_SIGNAL_MACRO)
    check_symbol_exists(uname          "${CURL_INCLUDES}" HAVE_UNAME)
    check_symbol_exists(strtoll        "${CURL_INCLUDES}" HAVE_STRTOLL)
    check_symbol_exists(_strtoi64      "${CURL_INCLUDES}" HAVE__STRTOI64)
    check_symbol_exists(strerror_r     "${CURL_INCLUDES}" HAVE_STRERROR_R)
    check_symbol_exists(siginterrupt   "${CURL_INCLUDES}" HAVE_SIGINTERRUPT)
    check_symbol_exists(perror         "${CURL_INCLUDES}" HAVE_PERROR)
    check_symbol_exists(fork           "${CURL_INCLUDES}" HAVE_FORK)
    check_symbol_exists(getaddrinfo    "${CURL_INCLUDES}" HAVE_GETADDRINFO)
    check_symbol_exists(freeaddrinfo   "${CURL_INCLUDES}" HAVE_FREEADDRINFO)
    check_symbol_exists(freeifaddrs    "${CURL_INCLUDES}" HAVE_FREEIFADDRS)
    check_symbol_exists(pipe           "${CURL_INCLUDES}" HAVE_PIPE)
    check_symbol_exists(ftruncate      "${CURL_INCLUDES}" HAVE_FTRUNCATE)
    check_symbol_exists(getprotobyname "${CURL_INCLUDES}" HAVE_GETPROTOBYNAME)
    check_symbol_exists(getrlimit      "${CURL_INCLUDES}" HAVE_GETRLIMIT)
    check_symbol_exists(setlocale      "${CURL_INCLUDES}" HAVE_SETLOCALE)
    check_symbol_exists(setmode        "${CURL_INCLUDES}" HAVE_SETMODE)
    check_symbol_exists(setrlimit      "${CURL_INCLUDES}" HAVE_SETRLIMIT)
    check_symbol_exists(fcntl          "${CURL_INCLUDES}" HAVE_FCNTL)
    check_symbol_exists(ioctl          "${CURL_INCLUDES}" HAVE_IOCTL)
    check_symbol_exists(setsockopt     "${CURL_INCLUDES}" HAVE_SETSOCKOPT)

    if(HAVE_SIZEOF_LONG_LONG)
      set(HAVE_LONGLONG 1)
      set(HAVE_LL 1)
    endif(HAVE_SIZEOF_LONG_LONG)

    check_function_exists(mach_absolute_time HAVE_MACH_ABSOLUTE_TIME)
    check_function_exists(gethostname HAVE_GETHOSTNAME)

    check_include_file_concat("pthread.h" HAVE_PTHREAD_H)
    check_symbol_exists(recv "sys/socket.h" HAVE_RECV)
    check_symbol_exists(send "sys/socket.h" HAVE_SEND)

    check_struct_has_member("struct sockaddr_un" sun_path "sys/un.h" USE_UNIX_SOCKETS)

    set(CMAKE_REQUIRED_INCLUDES "${CURL_SOURCES_DIR}/include")
    set(CMAKE_EXTRA_INCLUDE_FILES "curl/system.h")
    check_type_size("curl_off_t"  SIZEOF_CURL_OFF_T)

    add_definitions(-DHAVE_GLIBC_STRERROR_R=1)

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
        HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID
        TIME_WITH_SYS_TIME
        HAVE_O_NONBLOCK
        HAVE_GETHOSTBYADDR_R_5
        HAVE_GETHOSTBYADDR_R_7
        HAVE_GETHOSTBYADDR_R_8
        HAVE_GETHOSTBYADDR_R_5_REENTRANT
        HAVE_GETHOSTBYADDR_R_7_REENTRANT
        HAVE_GETHOSTBYADDR_R_8_REENTRANT
        HAVE_GETHOSTBYNAME_R_3
        HAVE_GETHOSTBYNAME_R_5
        HAVE_GETHOSTBYNAME_R_6
        HAVE_GETHOSTBYNAME_R_3_REENTRANT
        HAVE_GETHOSTBYNAME_R_5_REENTRANT
        HAVE_GETHOSTBYNAME_R_6_REENTRANT
        HAVE_SOCKLEN_T
        HAVE_IN_ADDR_T
        HAVE_BOOL_T
        STDC_HEADERS
        RETSIGTYPE_TEST
        HAVE_INET_NTOA_R_DECL
        HAVE_INET_NTOA_R_DECL_REENTRANT
        HAVE_GETADDRINFO
        HAVE_FILE_OFFSET_BITS
        )
      curl_internal_test(${CURL_TEST})
    endforeach(CURL_TEST)

    configure_file(
      ${CURL_SOURCES_DIR}/lib/curl_config.h.cmake
      ${CURL_SOURCES_DIR}/lib/curl_config.h
      )
  endif()
else()
  include(FindCURL)
  include_directories(${CURL_INCLUDE_DIRS})
  link_libraries(${CURL_LIBRARIES})

  if (NOT ${CURL_FOUND})
    message(FATAL_ERROR "Unable to find LibCurl")
  endif()
endif()
