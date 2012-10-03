if (${STATIC_BUILD})
  SET(GOOGLE_LOG_SOURCES_DIR ${CMAKE_BINARY_DIR}/glog-0.3.2)
  DownloadPackage("http://google-glog.googlecode.com/files/glog-0.3.2.tar.gz" "${GOOGLE_LOG_SOURCES_DIR}" "" "")

  include_directories(
    ${GOOGLE_LOG_SOURCES_DIR}/src
    )

  if (CMAKE_COMPILER_IS_GNUCXX)
    execute_process(
      COMMAND patch utilities.cc ${CMAKE_SOURCE_DIR}/Resources/Patches/glog-utilities.diff
      WORKING_DIRECTORY ${GOOGLE_LOG_SOURCES_DIR}/src
      )
    execute_process(
      COMMAND patch port.h ${CMAKE_SOURCE_DIR}/Resources/Patches/glog-port.diff 
      WORKING_DIRECTORY ${GOOGLE_LOG_SOURCES_DIR}/src/windows
      )
  endif()

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    configure_file(
      ${CMAKE_SOURCE_DIR}/Resources/CMake/GoogleLogConfiguration.h
      ${GOOGLE_LOG_SOURCES_DIR}/src/config.h
      COPYONLY)

    set(GOOGLE_LOG_SOURCES
      ${GOOGLE_LOG_SOURCES_DIR}/src/demangle.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/logging.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/raw_logging.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/signalhandler.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/symbolize.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/utilities.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/vlog_is_on.cc
      )

  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    include_directories(
      ${GOOGLE_LOG_SOURCES_DIR}/src/windows
      )

    set(GOOGLE_LOG_SOURCES
      ${GOOGLE_LOG_SOURCES_DIR}/src/logging.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/windows/port.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/raw_logging.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/utilities.cc
      ${GOOGLE_LOG_SOURCES_DIR}/src/vlog_is_on.cc
      )

    add_definitions(
      -DGLOG_NO_ABBREVIATED_SEVERITIES=1
      -DNO_FRAME_POINTER=1
      -DGOOGLE_GLOG_DLL_DECL=
      )
  endif()
 
  add_library(GoogleLog STATIC ${GOOGLE_LOG_SOURCES})
  link_libraries(GoogleLog)

else()
  CHECK_INCLUDE_FILE_CXX(glog/logging.h HAVE_GOOGLE_LOG_H)
  if (NOT HAVE_GOOGLE_LOG_H)
    message(FATAL_ERROR "Please install the libgoogle-glog-dev package")
  endif()

  link_libraries(glog)
endif()
