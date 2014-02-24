if (STATIC_BUILD OR NOT USE_SYSTEM_BOOST)
  set(BOOST_STATIC 1)
else()
  include(FindBoost)

  set(BOOST_STATIC 0)
  #set(Boost_DEBUG 1)
  #set(Boost_USE_STATIC_LIBS ON)

  find_package(Boost
    COMPONENTS filesystem thread system date_time regex)

  if (NOT Boost_FOUND)
    message(FATAL_ERROR "Unable to locate Boost on this system")
  endif()

  # Boost releases 1.44 through 1.47 supply both V2 and V3 filesystem
  # http://www.boost.org/doc/libs/1_46_1/libs/filesystem/v3/doc/index.htm
  if (${Boost_VERSION} LESS 104400)
    add_definitions(
      -DBOOST_HAS_FILESYSTEM_V3=0
      )
  else()
    add_definitions(
      -DBOOST_HAS_FILESYSTEM_V3=1
      -DBOOST_FILESYSTEM_VERSION=3
      )
  endif()

  #if (${Boost_VERSION} LESS 104800)
  # boost::locale is only available from 1.48.00
  #message("Too old version of Boost (${Boost_LIB_VERSION}): Building the static version")
  #  set(BOOST_STATIC 1)
  #endif()

  include_directories(${Boost_INCLUDE_DIRS})
  link_libraries(${Boost_LIBRARIES})
endif()


if (BOOST_STATIC)
  # Parameters for Boost 1.54.0
  set(BOOST_NAME boost_1_54_0)
  set(BOOST_BCP_SUFFIX bcpdigest-0.6.2)
  set(BOOST_MD5 "a464288a976ba133f9b325f454cb503d")
  set(BOOST_FILESYSTEM_SOURCES_DIR "${BOOST_NAME}/libs/filesystem/src")
  
  set(BOOST_SOURCES_DIR ${CMAKE_BINARY_DIR}/${BOOST_NAME})
  DownloadPackage(
    "${BOOST_MD5}"
    "http://www.montefiore.ulg.ac.be/~jodogne/Orthanc/ThirdPartyDownloads/${BOOST_NAME}_${BOOST_BCP_SUFFIX}.tar.gz"
    "${BOOST_SOURCES_DIR}"
    )

  set(BOOST_SOURCES)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    list(APPEND BOOST_SOURCES
      ${BOOST_SOURCES_DIR}/libs/thread/src/pthread/once.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/pthread/thread.cpp
      )
    add_definitions(
      -DBOOST_LOCALE_WITH_ICONV=1
      )

    if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
      add_definitions(-DBOOST_HAS_SCHED_YIELD=1)
    endif()

  elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(APPEND BOOST_SOURCES
      ${BOOST_SOURCES_DIR}/libs/thread/src/win32/tss_dll.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/win32/thread.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/win32/tss_pe.cpp
      ${BOOST_FILESYSTEM_SOURCES_DIR}/windows_file_codecvt.cpp
      )
    add_definitions(
      -DBOOST_LOCALE_WITH_WCONV=1
      )
  else()
    message(FATAL_ERROR "Support your platform here")
  endif()

  aux_source_directory(${BOOST_SOURCES_DIR}/libs/regex/src BOOST_REGEX_SOURCES)

  list(APPEND BOOST_SOURCES
    ${BOOST_REGEX_SOURCES}
    ${BOOST_SOURCES_DIR}/libs/date_time/src/gregorian/greg_month.cpp
    ${BOOST_FILESYSTEM_SOURCES_DIR}/codecvt_error_category.cpp
    ${BOOST_FILESYSTEM_SOURCES_DIR}/operations.cpp
    ${BOOST_FILESYSTEM_SOURCES_DIR}/path.cpp
    ${BOOST_FILESYSTEM_SOURCES_DIR}/path_traits.cpp
    ${BOOST_SOURCES_DIR}/libs/locale/src/encoding/codepage.cpp
    ${BOOST_SOURCES_DIR}/libs/system/src/error_code.cpp
    )

  list(APPEND THIRD_PARTY_SOURCES ${BOOST_SOURCES})

  add_definitions(
    # Static build of Boost
    -DBOOST_ALL_NO_LIB 
    -DBOOST_ALL_NOLIB 
    -DBOOST_DATE_TIME_NO_LIB 
    -DBOOST_THREAD_BUILD_LIB
    -DBOOST_PROGRAM_OPTIONS_NO_LIB
    -DBOOST_REGEX_NO_LIB
    -DBOOST_SYSTEM_NO_LIB
    -DBOOST_LOCALE_NO_LIB
    -DBOOST_HAS_LOCALE=1
    -DBOOST_HAS_FILESYSTEM_V3=1
    )

  if (${CMAKE_COMPILER_IS_GNUCXX})
    add_definitions(-isystem ${BOOST_SOURCES_DIR})
  endif()

  include_directories(
    ${BOOST_SOURCES_DIR}
    )

  source_group(ThirdParty\\Boost REGULAR_EXPRESSION ${BOOST_SOURCES_DIR}/.*)
else()
  add_definitions(
    -DBOOST_HAS_LOCALE=0
    )
endif()
