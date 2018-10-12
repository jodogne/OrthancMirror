if (STATIC_BUILD OR NOT USE_SYSTEM_MONGOOSE)
  SET(MONGOOSE_SOURCES_DIR ${CMAKE_BINARY_DIR}/mongoose)

  if (IS_DIRECTORY "${MONGOOSE_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()

  if (0)
    # Use Mongoose 3.1
    DownloadPackage(
      "e718fc287b4eb1bd523be3fa00942bb0"
      "http://www.orthanc-server.com/downloads/third-party/mongoose-3.1.tgz"
      "${MONGOOSE_SOURCES_DIR}")
    
    add_definitions(-DMONGOOSE_USE_CALLBACKS=0)
    set(MONGOOSE_PATCH ${ORTHANC_ROOT}/Resources/Patches/mongoose-3.1-patch.diff)

  else() 
    # Use Mongoose 3.8
    DownloadPackage(
      "7e3296295072792cdc3c633f9404e0c3"
      "http://www.orthanc-server.com/downloads/third-party/mongoose-3.8.tgz"
      "${MONGOOSE_SOURCES_DIR}")
    
    add_definitions(-DMONGOOSE_USE_CALLBACKS=1)
    set(MONGOOSE_PATCH ${ORTHANC_ROOT}/Resources/Patches/mongoose-3.8-patch.diff)
  endif()

  # Patch mongoose
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -N mongoose.c 
    INPUT_FILE ${MONGOOSE_PATCH}
    WORKING_DIRECTORY ${MONGOOSE_SOURCES_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure AND FirstRun)
    message(FATAL_ERROR "Error while patching a file")
  endif()

  include_directories(
    ${MONGOOSE_SOURCES_DIR}
    )

  set(MONGOOSE_SOURCES
    ${MONGOOSE_SOURCES_DIR}/mongoose.c
    )


  if (ENABLE_SSL)
    add_definitions(
      -DNO_SSL_DL=1
      )
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
        ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
      link_libraries(dl)
    endif()

  else()
    add_definitions(
      -DNO_SSL=1   # Remove SSL support from mongoose
      )
  endif()


  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    if (CMAKE_COMPILER_IS_GNUCXX)
      # This is a patch for MinGW64
      add_definitions(-D_TIMESPEC_DEFINED=1)
    endif()
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

  if (SYSTEM_MONGOOSE_USE_CALLBACKS)
    add_definitions(-DMONGOOSE_USE_CALLBACKS=1)
  else()
    add_definitions(-DMONGOOSE_USE_CALLBACKS=0)
  endif()

  link_libraries(mongoose)
endif()
