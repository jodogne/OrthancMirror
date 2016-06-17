if (STATIC_BUILD OR NOT USE_SYSTEM_CURL)
  SET(CURL_SOURCES_DIR ${CMAKE_BINARY_DIR}/curl-7.44.0)
  SET(CURL_URL "http://www.montefiore.ulg.ac.be/~jodogne/Orthanc/ThirdPartyDownloads/curl-7.44.0.tar.gz")
  SET(CURL_MD5 "cf46112b5151e2f1a3fd38439bdade23")

  DownloadPackage(${CURL_MD5} ${CURL_URL} "${CURL_SOURCES_DIR}")

  include_directories(
    ${CURL_SOURCES_DIR}/include
    )

  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib CURL_SOURCES)
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

  file(WRITE ${CURL_SOURCES_DIR}/lib/curl_config.h "")

  file(GLOB CURL_LIBS_HEADERS ${CURL_SOURCES_DIR}/lib/*.h)
  foreach (header IN LISTS CURL_LIBS_HEADERS)
    get_filename_component(filename ${header} NAME)
    file(WRITE ${CURL_SOURCES_DIR}/lib/vtls/${filename} "#include \"../${filename}\"\n")
  endforeach()

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
    if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      SET(TMP_OS "x86_64")
    else()
      SET(TMP_OS "x86")
    endif()

    set_property(
      SOURCE ${CURL_SOURCES}
      PROPERTY COMPILE_DEFINITIONS "HAVE_TIME_H;HAVE_STRUCT_TIMEVAL;HAVE_SYS_STAT_H;HAVE_SOCKET;HAVE_STRUCT_SOCKADDR_STORAGE;HAVE_SYS_SOCKET_H;HAVE_SOCKET;HAVE_SYS_SOCKET_H;HAVE_NETINET_IN_H;HAVE_NETDB_H;HAVE_FCNTL_O_NONBLOCK;HAVE_FCNTL_H;HAVE_SELECT;HAVE_ERRNO_H;HAVE_SEND;HAVE_RECV;HAVE_LONGLONG;OS=\"${TMP_OS}\""
      )

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
