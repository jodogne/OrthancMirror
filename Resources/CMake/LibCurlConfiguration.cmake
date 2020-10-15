if (STATIC_BUILD OR NOT USE_SYSTEM_CURL)
  SET(CURL_SOURCES_DIR ${CMAKE_BINARY_DIR}/curl-7.26.0)
  DownloadPackage(
    "3fa4d5236f2a36ca5c3af6715e837691"
    "http://www.orthanc-server.com/downloads/third-party/curl-7.26.0.tar.gz"
    "${CURL_SOURCES_DIR}")

  include_directories(${CURL_SOURCES_DIR}/include)
  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib CURL_SOURCES)
  source_group(ThirdParty\\LibCurl REGULAR_EXPRESSION ${CURL_SOURCES_DIR}/.*)

  #add_library(Curl STATIC ${CURL_SOURCES})
  #link_libraries(Curl)  

  add_definitions(
    -DCURL_STATICLIB=1
    -DBUILDING_LIBCURL=1
    -DCURL_DISABLE_LDAPS=1
    -DCURL_DISABLE_LDAP=1
    -D_WIN32_WINNT=0x0501

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

  if (${ENABLE_SSL})
    add_definitions(
      #-DHAVE_LIBSSL=1
      -DUSE_OPENSSL=1
      -DUSE_SSLEAY=1
      )
  endif()

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
    if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      SET(TMP_OS "x86_64")
    else()
      SET(TMP_OS "x86")
    endif()

    set_property(
      SOURCE ${CURL_SOURCES}
      PROPERTY COMPILE_DEFINITIONS "HAVE_TIME_H;HAVE_STRUCT_TIMEVAL;HAVE_SYS_STAT_H;HAVE_SOCKET;HAVE_STRUCT_SOCKADDR_STORAGE;HAVE_SYS_SOCKET_H;HAVE_SOCKET;HAVE_SYS_SOCKET_H;HAVE_NETINET_IN_H;HAVE_NETDB_H;HAVE_FCNTL_O_NONBLOCK;HAVE_FCNTL_H;HAVE_SELECT;HAVE_ERRNO_H;HAVE_SEND;HAVE_RECV;OS=\"${TMP_OS}\"")

    if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      add_definitions(
        -DRECV_TYPE_ARG1=int
        -DRECV_TYPE_ARG2=void*
        -DRECV_TYPE_ARG3=size_t
        -DRECV_TYPE_ARG4=int
        -DRECV_TYPE_RETV=ssize_t
        -DSEND_TYPE_ARG1=int
        -DSEND_TYPE_ARG2=void*
        -DSEND_QUAL_ARG2=const
        -DSEND_TYPE_ARG3=size_t
        -DSEND_TYPE_ARG4=int
        -DSEND_TYPE_RETV=ssize_t
        -DSIZEOF_SHORT=2
        -DSIZEOF_INT=4
        -DSIZEOF_SIZE_T=8
        )
    elseif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
      add_definitions(
        -DRECV_TYPE_ARG1=int
        -DRECV_TYPE_ARG2=void*
        -DRECV_TYPE_ARG3=size_t
        -DRECV_TYPE_ARG4=int
        -DRECV_TYPE_RETV=int
        -DSEND_TYPE_ARG1=int
        -DSEND_TYPE_ARG2=void*
        -DSEND_QUAL_ARG2=const
        -DSEND_TYPE_ARG3=size_t
        -DSEND_TYPE_ARG4=int
        -DSEND_TYPE_RETV=int
        -DSIZEOF_SHORT=2
        -DSIZEOF_INT=4
        -DSIZEOF_SIZE_T=4
        )
    else()
      message(FATAL_ERROR "Support your platform here")
    endif()
  endif()

else()
  include(FindCURL)
  include_directories(${CURL_INCLUDE_DIRS})
  link_libraries(${CURL_LIBRARIES})

  if (NOT ${CURL_FOUND})
    message(FATAL_ERROR "Unable to find LibCurl")
  endif()
endif()
