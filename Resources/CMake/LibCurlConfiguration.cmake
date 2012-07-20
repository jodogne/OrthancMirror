if (${STATIC_BUILD})
  SET(CURL_SOURCES_DIR ${CMAKE_BINARY_DIR}/curl-7.26.0)
  DownloadPackage("http://curl.haxx.se/download/curl-7.26.0.tar.gz" "${CURL_SOURCES_DIR}" "" "")

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      SET(SOURCE_CONFIG ${CMAKE_SOURCE_DIR}/Resources/libcurl/x86_64-linux)
    elseif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
      SET(SOURCE_CONFIG ${CMAKE_SOURCE_DIR}/Resources/libcurl/i686-pc-linux-gnu)
    else()
      message(FATAL_ERROR "Support your platform here")
    endif()
  elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    if (${MSVC})
      SET(SOURCE_CONFIG ${CMAKE_SOURCE_DIR}/Resources/libcurl/msvc)
    else()
      SET(SOURCE_CONFIG ${CMAKE_SOURCE_DIR}/Resources/libcurl/i586-mingw32msvc)
    endif()
  else()
    message(FATAL_ERROR "Support your platform here")
  endif()

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    configure_file("${SOURCE_CONFIG}/curl_config.h" "${CURL_SOURCES_DIR}/lib/curl_config.h" COPYONLY)
    configure_file("${SOURCE_CONFIG}/curlbuild.h" "${CURL_SOURCES_DIR}/include/curl/curlbuild.h" COPYONLY)
  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    configure_file("${SOURCE_CONFIG}/curlbuild.h" "${CURL_SOURCES_DIR}/include/curl/curlbuild.h" COPYONLY)
  endif()

  include_directories(${CURL_SOURCES_DIR}/include)

  AUX_SOURCE_DIRECTORY(${CURL_SOURCES_DIR}/lib CURL_SOURCES)
  source_group(ThirdParty\\LibCurl REGULAR_EXPRESSION ${CURL_SOURCES_DIR}/.*)

  list(APPEND THIRD_PARTY_SOURCES ${CURL_SOURCES})
  
  add_definitions(
    -DCURL_STATICLIB=1
    -DBUILDING_LIBCURL=1
    -DCURL_DISABLE_LDAPS=1
    -DCURL_DISABLE_LDAP=1
    -D_WIN32_WINNT=0x0501
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set_property(
      SOURCE ${CURL_SOURCES}
      PROPERTY COMPILE_DEFINITIONS HAVE_CONFIG_H)
  endif()

else()
  include(FindCURL)
  include_directories(${CURL_INCLUDE_DIRS})
  link_libraries(${CURL_LIBRARIES})

  if (NOT ${CURL_FOUND})
    message(FATAL_ERROR "Unable to find LibCurl")
  endif()
endif()
