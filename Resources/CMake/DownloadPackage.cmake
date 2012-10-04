macro(GetUrlFilename TargetVariable Url)
  string(REGEX REPLACE "^.*/" "" ${TargetVariable} "${Url}")
endmacro()


macro(GetUrlExtension TargetVariable Url)
  #string(REGEX REPLACE "^.*/[^.]*\\." "" TMP "${Url}")
  string(REGEX REPLACE "^.*\\." "" TMP "${Url}")
  string(TOLOWER "${TMP}" "${TargetVariable}")
endmacro()


macro(DownloadPackage Url TargetDirectory PreloadedVariable UncompressArguments)
  if (NOT IS_DIRECTORY "${TargetDirectory}")
    GetUrlFilename(TMP_FILENAME "${Url}")
    if ("${PreloadedVariable}" STREQUAL "")
      set(TMP_PATH "${CMAKE_SOURCE_DIR}/ThirdPartyDownloads/${TMP_FILENAME}")
      if (NOT EXISTS "${TMP_PATH}")
        message("Downloading ${Url}")
        file(DOWNLOAD "${Url}" "${TMP_PATH}" SHOW_PROGRESS)
      else()
        message("Using local copy of ${Url}")
      endif()
    else()
      message("Using preloaded archive ${PreloadedVariable} for ${Url}")
      set(TMP_PATH "${PreloadedVariable}")
    endif()

    GetUrlExtension(TMP_EXTENSION "${Url}")
    #message(${TMP_EXTENSION})
    message("Uncompressing ${TMP_FILENAME}")

    if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
      if ("${TMP_EXTENSION}" STREQUAL "zip")
        execute_process(
          COMMAND sh -c "unzip -q ${TMP_PATH} ${UncompressArguments}"
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          RESULT_VARIABLE Failure
        )
      elseif (("${TMP_EXTENSION}" STREQUAL "gz") OR ("${TMP_EXTENSION}" STREQUAL "tgz"))
        #message("tar xvfz ${TMP_PATH} ${UncompressArguments}")
        execute_process(
          COMMAND sh -c "tar xfz ${TMP_PATH} ${UncompressArguments}"
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          RESULT_VARIABLE Failure
          )
      elseif ("${TMP_EXTENSION}" STREQUAL "bz2")
        execute_process(
          COMMAND sh -c "tar xfj ${TMP_PATH} ${UncompressArguments}"
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          RESULT_VARIABLE Failure
          )
      else()
        message(FATAL_ERROR "Unknown package format.")
      endif()

    elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
      # How to silently extract files using 7-zip
      # http://superuser.com/questions/331148/7zip-command-line-extract-silently-quietly

      FIND_PROGRAM(ZIP_EXECUTABLE 7z PATHS "$ENV{ProgramFiles}/7-Zip") 

      if (("${TMP_EXTENSION}" STREQUAL "gz") OR ("${TMP_EXTENSION}" STREQUAL "tgz"))
        execute_process(
          COMMAND ${ZIP_EXECUTABLE} e -y ${TMP_PATH}
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          RESULT_VARIABLE Failure
          OUTPUT_QUIET
          )

        if (Failure)
          message(FATAL_ERROR "Error while running the uncompression tool")
        endif()

        set(ARGS ${UncompressArguments})
        SEPARATE_ARGUMENTS(ARGS)
        list(LENGTH ARGS TMP_LENGTH)

        if ("${TMP_EXTENSION}" STREQUAL "tgz")
          string(REGEX REPLACE ".tgz$" ".tar" TMP_FILENAME2 "${TMP_FILENAME}")
        else()
          string(REGEX REPLACE ".gz$" "" TMP_FILENAME2 "${TMP_FILENAME}")
        endif()

        if (TMP_LENGTH EQUAL 0)
          execute_process(
            COMMAND ${ZIP_EXECUTABLE} x -y ${TMP_FILENAME2}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            RESULT_VARIABLE Failure
            OUTPUT_QUIET
            )
        else()
          foreach(SUBDIR ${ARGS})
            execute_process(
              COMMAND ${ZIP_EXECUTABLE} x -y "-i!${SUBDIR}" "${TMP_FILENAME2}"
              WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
              RESULT_VARIABLE Failure
              OUTPUT_QUIET
              )

            if (Failure)
              message(FATAL_ERROR "Error while running the uncompression tool")
            endif()
          endforeach()
        endif()
      elseif ("${TMP_EXTENSION}" STREQUAL "zip")
        execute_process(
          COMMAND ${ZIP_EXECUTABLE} x -y ${TMP_PATH}
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          RESULT_VARIABLE Failure
          OUTPUT_QUIET
          )
      else()
        message(FATAL_ERROR "Support your platform here")
      endif()
    else()
      message(FATAL_ERROR "Support your platform here")
    endif()
   
    if (Failure)
      message(FATAL_ERROR "Error while running the uncompression tool")
    endif()

    if (NOT IS_DIRECTORY "${TargetDirectory}")
      message(FATAL_ERROR "The package was not uncompressed at the proper location. Check the CMake instructions.")
    endif()
  endif()
endmacro()

