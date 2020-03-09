if (STATIC_BUILD OR NOT USE_SYSTEM_OPENSSL)
  if (OPENSSL_STATIC_VERSION STREQUAL "1.0.2")
    include(${CMAKE_CURRENT_LIST_DIR}/OpenSslConfigurationStatic-1.0.2.cmake)
  elseif (OPENSSL_STATIC_VERSION STREQUAL "1.1.1")
    include(${CMAKE_CURRENT_LIST_DIR}/OpenSslConfigurationStatic-1.1.1.cmake)
  else()
    message(FATAL_ERROR "Unsupported version of OpenSSL: ${OPENSSL_STATIC_VERSION}")
  endif()

  source_group(ThirdParty\\OpenSSL REGULAR_EXPRESSION ${OPENSSL_SOURCES_DIR}/.*)

else()
  include(FindOpenSSL)

  if (NOT ${OPENSSL_FOUND})
    message(FATAL_ERROR "Unable to find OpenSSL")
  endif()

  include_directories(${OPENSSL_INCLUDE_DIR})
  link_libraries(${OPENSSL_LIBRARIES})
endif()
