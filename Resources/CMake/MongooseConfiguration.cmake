SET(MONGOOSE_SOURCES_DIR ${CMAKE_BINARY_DIR}/mongoose)
DownloadPackage("http://mongoose.googlecode.com/files/mongoose-3.1.tgz" "${MONGOOSE_SOURCES_DIR}" "" "")

# Patch mongoose
execute_process(
  COMMAND patch mongoose.c ${CMAKE_SOURCE_DIR}/Resources/mongoose-patch.diff
  WORKING_DIRECTORY ${MONGOOSE_SOURCES_DIR}
  )

include_directories(
  ${MONGOOSE_SOURCES_DIR}
  )

list(APPEND THIRD_PARTY_SOURCES
  ${MONGOOSE_SOURCES_DIR}/mongoose.c
  )

add_definitions(
  # Remove SSL support from mongoose
  -DNO_SSL=1
  )

source_group(ThirdParty\\Mongoose REGULAR_EXPRESSION ${MONGOOSE_SOURCES_DIR}/.*)
