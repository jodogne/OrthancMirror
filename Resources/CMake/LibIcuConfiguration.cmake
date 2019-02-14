message("Using libicu")

if (STATIC_BUILD OR NOT USE_SYSTEM_LIBICU)
  # set(LIBICU_SOURCES_DIR ${CMAKE_BINARY_DIR}/libicu-1.15)
  # set(LIBICU_URL "http://orthanc.osimis.io/ThirdPartyDownloads/libicu-1.15.tar.gz")
  # set(LIBICU_MD5 "ace8b5f2db42f7b3b3057585e80d9808")

  # DownloadPackage(${LIBICU_MD5} ${LIBICU_URL} "${LIBICU_SOURCES_DIR}")

  # # Disable the support of libicu that is shipped by default with
  # # the C standard library on Linux. Setting this macro redirects
  # # calls from "icu*()" to "libicu*()" by defining macros in the
  # # C headers of "libicu-1.15".
  # add_definitions(-DLIBICU_PLUG=1)

  # # https://groups.google.com/d/msg/android-ndk/AS1nkxnk6m4/EQm09hD1tigJ
  # add_definitions(
  #   -DBUILDING_LIBICU=1
  #   -DIN_LIBRARY=1
  #   -DLIBDIR=""
  #   -DICU_CONST=
  #   )

  # configure_file(
  #   ${LIBICU_SOURCES_DIR}/srclib/localcharset.h
  #   ${LIBICU_SOURCES_DIR}/include
  #   COPYONLY)

  # set(HAVE_VISIBILITY 0)
  # set(ICU_CONST ${ICU_CONST})
  # set(USE_MBSTATE_T 1)
  # set(BROKEN_WCHAR_H 0)
  # set(EILSEQ)
  # set(HAVE_WCHAR_T 1)
  # configure_file(
  #   ${LIBICU_SOURCES_DIR}/include/icu.h.build.in
  #   ${LIBICU_SOURCES_DIR}/include/icu.h
  #   )
  # unset(HAVE_VISIBILITY)
  # unset(ICU_CONST)
  # unset(USE_MBSTATE_T)
  # unset(BROKEN_WCHAR_H)
  # unset(EILSEQ)
  # unset(HAVE_WCHAR_T)

  # if (NOT EXISTS ${LIBICU_SOURCES_DIR}/include/config.h)
  #   # Create an empty "config.h" for libicu
  #   file(WRITE ${LIBICU_SOURCES_DIR}/include/config.h "")
  # endif()

  # include_directories(
  #   ${LIBICU_SOURCES_DIR}/include
  #   )

  # set(LIBICU_SOURCES
  #   ${LIBICU_SOURCES_DIR}/lib/icu.c  
  #   ${LIBICU_SOURCES_DIR}/lib/relocatable.c
  #   ${LIBICU_SOURCES_DIR}/libcharset/lib/localcharset.c  
  #   ${LIBICU_SOURCES_DIR}/libcharset/lib/relocatable.c
  #   )

  # source_group(ThirdParty\\libicu REGULAR_EXPRESSION ${LIBICU_SOURCES_DIR}/.*)

  # if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  #   add_definitions(-DHAVE_WORKING_O_NOFOLLOW=0)
  # else()
  #   add_definitions(-DHAVE_WORKING_O_NOFOLLOW=1)
  # endif()

else() 
  CHECK_INCLUDE_FILE_CXX(unicode/uvernum.h HAVE_ICU_H)
  if (NOT HAVE_ICU_H)
    message(FATAL_ERROR "Please install the libicu-dev package")
  endif()

  CHECK_LIBRARY_EXISTS(icuuc udata_close "" HAVE_ICU_LIB)
  if (NOT HAVE_ICU_LIB)
    #message(FATAL_ERROR "Please install the libicu-dev package")
    link_libraries(icuuc)
  else()
    link_libraries(icuuc)
  endif()
endif()
