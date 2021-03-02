# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program. If not, see
# <http://www.gnu.org/licenses/>.



# Check out: ../ThirdParty/icu/README.txt

# http://userguide.icu-project.org/packaging
# http://userguide.icu-project.org/howtouseicu

message("Using libicu")

if (STATIC_BUILD OR NOT USE_SYSTEM_LIBICU)
  include(${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/icu/Version.cmake)
  DownloadPackage(${LIBICU_MD5} ${LIBICU_URL} "${LIBICU_SOURCES_DIR}")

  # Use the gzip-compressed data
  DownloadFile(${LIBICU_DATA_COMPRESSED_MD5} ${LIBICU_DATA_URL})
  set(LIBICU_RESOURCES
    LIBICU_DATA  ${CMAKE_SOURCE_DIR}/ThirdPartyDownloads/${LIBICU_DATA}
    )

  set_source_files_properties(
    ${CMAKE_BINARY_DIR}/${LIBICU_DATA}
    PROPERTIES COMPILE_DEFINITIONS "char16_t=uint16_t"
    )

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
    -DU_STATIC_IMPLEMENTATION
    -DU_ENABLE_DYLOAD=0
    -DU_HAVE_STD_STRING=1
    -DU_I18N_IMPLEMENTATION
    -DU_IO_IMPLEMENTATION
    -DU_STATIC_IMPLEMENTATION=1
    #-DU_CHARSET_IS_UTF8
    -DUNISTR_FROM_STRING_EXPLICIT=

    -DORTHANC_STATIC_ICU=1
    -DORTHANC_ICU_DATA_MD5="${LIBICU_DATA_UNCOMPRESSED_MD5}"
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
    link_libraries(${LIBICU_PATH_1} ${LIBICU_PATH_2})
  endif()

  add_definitions(
    -DORTHANC_STATIC_ICU=0
    )
endif()
