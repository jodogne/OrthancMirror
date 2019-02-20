
# Check out: ../ThirdParty/icu/README.txt

# http://userguide.icu-project.org/packaging
# http://userguide.icu-project.org/howtouseicu

message("Using libicu")

if (STATIC_BUILD OR NOT USE_SYSTEM_LIBICU)
  include(${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/icu/Version.cmake)
  DownloadPackage(${LIBICU_MD5} ${LIBICU_URL} "${LIBICU_SOURCES_DIR}")

  if (MSVC AND
      CMAKE_SIZEOF_VOID_P EQUAL 8)
    # In Visual Studio 2015 64bit, we get the following error if using
    # the plain C version of the ICU data: "icudt58l_dat.c(1638339):
    # fatal error C1060: compiler is out of heap space" => we use a
    # precompiled binary generated using MinGW on Linux
    DownloadCompressedFile(${LIBICU_DATA_WIN64_MD5} ${LIBICU_DATA_WIN64_URL} ${LIBICU_DATA_WIN64})

    set(LIBICU_LIBRARIES
      ${CMAKE_BINARY_DIR}/${LIBICU_DATA_WIN64}
      )
  else()
    # Use plain C data library
    DownloadCompressedFile(${LIBICU_DATA_MD5} ${LIBICU_DATA_URL} ${LIBICU_DATA})

    set_source_files_properties(
      ${CMAKE_BINARY_DIR}/${LIBICU_DATA}
      PROPERTIES COMPILE_DEFINITIONS "char16_t=uint16_t"
      )

    set(LIBICU_SOURCES
      ${CMAKE_BINARY_DIR}/${LIBICU_DATA}
      )
  endif()
  
  include_directories(BEFORE
    ${LIBICU_SOURCES_DIR}/source/common
    ${LIBICU_SOURCES_DIR}/source/i18n
    )

  aux_source_directory(${LIBICU_SOURCES_DIR}/source/common LIBICU_SOURCES)
  aux_source_directory(${LIBICU_SOURCES_DIR}/source/i18n LIBICU_SOURCES)

  add_definitions(
    #-DU_COMBINED_IMPLEMENTATION
    #-DU_DEF_ICUDATA_ENTRY_POINT=icudt63l_dat
    #-DU_LIB_SUFFIX_C_NAME=l

    #-DUCONFIG_NO_SERVICE=1
    -DU_COMMON_IMPLEMENTATION
    -DU_ENABLE_DYLOAD=0
    -DU_HAVE_STD_STRING=1
    -DU_I18N_IMPLEMENTATION
    -DU_IO_IMPLEMENTATION
    -DU_STATIC_IMPLEMENTATION=1
    #-DU_CHARSET_IS_UTF8
    -DUNISTR_FROM_STRING_EXPLICIT=
    )

  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_source_files_properties(
      ${LIBICU_SOURCES_DIR}/source/common/locmap.c
      PROPERTIES COMPILE_DEFINITIONS "LOCALE_SNAME=0x0000005c"
      )
  endif()

  source_group(ThirdParty\\libicu REGULAR_EXPRESSION ${LIBICU_SOURCES_DIR}/.*)

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
