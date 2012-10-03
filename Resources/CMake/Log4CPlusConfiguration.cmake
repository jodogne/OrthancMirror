add_definitions(
  -DDCMTK_BUNDLES_LOG4CPLUS=${DCMTK_BUNDLES_LOG4CPLUS}
  )

if (DCMTK_BUNDLES_LOG4CPLUS)
  message("DCMTK already bundles its own copy of Log4CPlus")

elseif (STATIC_BUILD)
  SET(LOG4CPLUS_SOURCES_DIR ${CMAKE_BINARY_DIR}/log4cplus-1.0.4.1)
  DownloadPackage("http://downloads.sourceforge.net/project/log4cplus/log4cplus-stable/1.0.4/log4cplus-1.0.4.1.tar.gz" "${LOG4CPLUS_SOURCES_DIR}" "" "")

  execute_process(
    COMMAND patch src/factory.cxx ${CMAKE_SOURCE_DIR}/Resources/log4cplus-patch.diff
    WORKING_DIRECTORY ${LOG4CPLUS_SOURCES_DIR}
    )

  AUX_SOURCE_DIRECTORY(${LOG4CPLUS_SOURCES_DIR}/src THIRD_PARTY_SOURCES)

  add_definitions(
    -DLOG4CPLUS_STATIC=1
    -DINSIDE_LOG4CPLUS=1
    -DLOG4CPLUS_DECLSPEC_EXPORT=
    -DLOG4CPLUS_DECLSPEC_IMPORT=
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_definitions(
      -DLOG4CPLUS_HAVE_STDIO_H=1
      -DLOG4CPLUS_HAVE_UNISTD_H=1
      -DLOG4CPLUS_HAVE_SYSLOG_H=1
      -DLOG4CPLUS_HAVE_NETDB_H=1
      -DLOG4CPLUS_HAVE_ERRNO_H=1
      -DLOG4CPLUS_HAVE_STAT=1
      -DLOG4CPLUS_HAVE_SYS_STAT_H=1
      )
    file(WRITE ${LOG4CPLUS_SOURCES_DIR}/include/log4cplus/config/defines.hxx "// Empty file")

    list(REMOVE_ITEM THIRD_PARTY_SOURCES 
      ${LOG4CPLUS_SOURCES_DIR}/src/socket-win32.cxx
      )

  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(REMOVE_ITEM THIRD_PARTY_SOURCES 
      ${LOG4CPLUS_SOURCES_DIR}/src/socket-unix.cxx
      )
    
  endif()

  include_directories(${LOG4CPLUS_SOURCES_DIR}/include)

  source_group(ThirdParty\\Log4Cplus REGULAR_EXPRESSION ${LOG4CPLUS_SOURCES_DIR}/.*)

else()
  message(FATAL_ERROR "Dynamic log4cplus")
  
endif()
