macro(CHECK_CURL_TYPE_EXISTS TYPE VARIABLE)
  check_type_size("${TYPE}" SIZEOF_TYPE) # LANGUAGE CXX)
  
  if (SIZEOF_TYPE)
    set(${VARIABLE} ON)
  else()
    set(${VARIABLE} OFF)
  endif()
endmacro()


if (STATIC_BUILD OR NOT USE_SYSTEM_CURL)
  SET(CURL_SOURCES_DIR ${CMAKE_BINARY_DIR}/curl-7.57.0)
  SET(CURL_URL "http://www.orthanc-server.com/downloads/third-party/curl-7.57.0.tar.gz")
  SET(CURL_MD5 "c7aab73aaf5e883ca1d7518f93649dc2")

  DownloadPackage(${CURL_MD5} ${CURL_URL} "${CURL_SOURCES_DIR}")

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

    include(CheckTypeSize)
    include(CheckIncludeFile)
    include(CheckSymbolExists)

    CHECK_INCLUDE_FILE(time.h HAVE_TIME_H)
    CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
    CHECK_INCLUDE_FILE(sys/socket.h HAVE_SYS_SOCKET_H)
    CHECK_INCLUDE_FILE(netinet/in.h HAVE_NETINET_IN_H)
    CHECK_INCLUDE_FILE(netdb.h HAVE_NETDB_H)
    CHECK_INCLUDE_FILE(fcntl.h HAVE_FCNTL_H)
    CHECK_INCLUDE_FILE(errno.h HAVE_ERRNO_H)
    CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
    CHECK_INCLUDE_FILE(stdio.h HAVE_STDIO_H)
    CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

    CHECK_CURL_TYPE_EXISTS("long long" HAVE_LONGLONG)
    
    check_symbol_exists(socket "sys/socket.h" HAVE_SOCKET)
    check_symbol_exists(recv "sys/socket.h" HAVE_RECV)
    check_symbol_exists(send "sys/socket.h" HAVE_SEND)
    check_symbol_exists(select "sys/select.h" HAVE_SELECT)

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

    set(CMAKE_REQUIRED_INCLUDES "${CURL_SOURCES_DIR}/include")
    set(CMAKE_EXTRA_INCLUDE_FILES "curl/system.h")
    check_type_size("curl_off_t"  SIZEOF_CURL_OFF_T)

    include(${CURL_SOURCES_DIR}/CMake/OtherTests.cmake)
    set(HAVE_STRUCT_TIMEVAL ON)  # TODO WHY IS THIS NECESSARY?
    set(HAVE_FCNTL_O_NONBLOCK ON)  # TODO WHY IS THIS NECESSARY?

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
