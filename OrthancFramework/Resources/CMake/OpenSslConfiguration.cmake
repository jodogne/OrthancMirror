if (STATIC_BUILD OR NOT USE_SYSTEM_OPENSSL)
  if (OPENSSL_STATIC_VERSION STREQUAL "1.0.2")
    include(${CMAKE_CURRENT_LIST_DIR}/OpenSslConfigurationStatic-1.0.2.cmake)
  elseif (OPENSSL_STATIC_VERSION STREQUAL "1.1.1")
    include(${CMAKE_CURRENT_LIST_DIR}/OpenSslConfigurationStatic-1.1.1.cmake)
  else()
    message(FATAL_ERROR "Unsupported version of OpenSSL: ${OPENSSL_STATIC_VERSION}")
  endif()

  source_group(ThirdParty\\OpenSSL REGULAR_EXPRESSION ${OPENSSL_SOURCES_DIR}/.*)

elseif (CMAKE_CROSSCOMPILING AND
    "${CMAKE_SYSTEM_VERSION}" STREQUAL "CrossToolNg")

  CHECK_INCLUDE_FILE_CXX(openssl/opensslv.h HAVE_OPENSSL_H)
  if (NOT HAVE_OPENSSL_H)
    message(FATAL_ERROR "Please install the libopenssl-dev package")
  endif()

  CHECK_LIBRARY_EXISTS(crypto "OPENSSL_init" "" HAVE_OPENSSL_CRYPTO_LIB)
  if (NOT HAVE_OPENSSL_CRYPTO_LIB)
    message(FATAL_ERROR "Please install the libopenssl package")
  endif()  
  
  CHECK_LIBRARY_EXISTS(ssl "SSL_library_init" "" HAVE_OPENSSL_SSL_LIB)
  if (NOT HAVE_OPENSSL_SSL_LIB)
    message(FATAL_ERROR "Please install the libopenssl package")
  endif()  
  
  link_libraries(crypto ssl)

else()
  include(FindOpenSSL)

  if (NOT ${OPENSSL_FOUND})
    message(FATAL_ERROR "Unable to find OpenSSL")
  endif()

  include_directories(${OPENSSL_INCLUDE_DIR})
  link_libraries(${OPENSSL_LIBRARIES})
endif()
