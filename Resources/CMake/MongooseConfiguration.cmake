if (STATIC_BUILD OR NOT USE_DYNAMIC_MONGOOSE)
  SET(MONGOOSE_SOURCES_DIR ${CMAKE_BINARY_DIR}/mongoose)
  DownloadPackage(
    "e718fc287b4eb1bd523be3fa00942bb0"
    "http://www.montefiore.ulg.ac.be/~jodogne/Orthanc/ThirdPartyDownloads/mongoose-3.1.tgz"
    "${MONGOOSE_SOURCES_DIR}" "" "")

  # Patch mongoose
  execute_process(
    COMMAND patch mongoose.c ${CMAKE_SOURCE_DIR}/Resources/Patches/mongoose-patch.diff
    WORKING_DIRECTORY ${MONGOOSE_SOURCES_DIR}
    )

  include_directories(
    ${MONGOOSE_SOURCES_DIR}
    )

  list(APPEND THIRD_PARTY_SOURCES
    ${MONGOOSE_SOURCES_DIR}/mongoose.c
    )


  if (${ENABLE_SSL})
    add_definitions(
      -DNO_SSL_DL=1
      )
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
      link_libraries(dl)
    endif()

  else()
    add_definitions(
      -DNO_SSL=1   # Remove SSL support from mongoose
      )
  endif()


  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows" AND ${CMAKE_COMPILER_IS_GNUCXX})
    # This is a patch for MinGW64
    add_definitions(-D_TIMESPEC_DEFINED=1)
  endif()

  source_group(ThirdParty\\Mongoose REGULAR_EXPRESSION ${MONGOOSE_SOURCES_DIR}/.*)

else()
  CHECK_INCLUDE_FILE_CXX(mongoose.h HAVE_MONGOOSE_H)
  if (NOT HAVE_MONGOOSE_H)
    message(FATAL_ERROR "Please install the mongoose-devel package")
  endif()

  CHECK_LIBRARY_EXISTS(mongoose mg_start "" HAVE_MONGOOSE_LIB)
  if (NOT HAVE_MONGOOSE_LIB)
    message(FATAL_ERROR "Please install the mongoose-devel package")
  endif()

  link_libraries(mongoose)
endif()
