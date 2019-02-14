
# http://userguide.icu-project.org/packaging
# http://userguide.icu-project.org/howtouseicu

message("Using libicu")

if (STATIC_BUILD OR NOT USE_SYSTEM_LIBICU)
  set(LIBICU_SOURCES_DIR ${CMAKE_BINARY_DIR}/icu)
  set(LIBICU_URL "http://orthanc.osimis.io/ThirdPartyDownloads/icu4c-63_1-src.tgz")
  set(LIBICU_MD5 "9e40f6055294284df958200e308bce50")

  DownloadPackage(${LIBICU_MD5} ${LIBICU_URL} "${LIBICU_SOURCES_DIR}")


  # TODO
  add_definitions(
    -DU_STATIC_IMPLEMENTATION
    #-DU_COMBINED_IMPLEMENTATION
    )


else() 
  CHECK_INCLUDE_FILE_CXX(unicode/uvernum.h HAVE_ICU_H)
  if (NOT HAVE_ICU_H)
    message(FATAL_ERROR "Please install the libicu-dev package")
  endif()

  find_library(LIBICU_PATH_1 NAMES icuuc)
  find_library(LIBICU_PATH_2 NAMES icui18n)

  if (NOT LIBICU_PATH_1 OR 
      NOT LIBICU_PATH_2)
    message(FATAL_ERROR "Please install the libicu-dev package")
  else()
    link_libraries(icuuc icui18n)
  endif()
endif()
