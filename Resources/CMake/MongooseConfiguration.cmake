SET(MONGOOSE_SOURCES_DIR ${CMAKE_BINARY_DIR}/mongoose)
DownloadPackage("http://mongoose.googlecode.com/files/mongoose-3.1.tgz" "${MONGOOSE_SOURCES_DIR}" "" "")

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

source_group(ThirdParty\\Mongoose REGULAR_EXPRESSION ${MONGOOSE_SOURCES_DIR}/.*)
