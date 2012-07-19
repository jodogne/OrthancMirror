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
        message("Already downloaded ${Url}")
      endif()
    else()
      message("Using preloaded archive ${PreloadedVariable} for ${Url}")
      set(TMP_PATH "${PreloadedVariable}")
    endif()

    GetUrlExtension(TMP_EXTENSION "${Url}")
    #message(${TMP_EXTENSION})

    if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
      if ("${TMP_EXTENSION}" STREQUAL "zip")
        execute_process(
          COMMAND sh -c "unzip ${TMP_PATH} ${UncompressArguments}"
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          )
      elseif (("${TMP_EXTENSION}" STREQUAL "gz") OR ("${TMP_EXTENSION}" STREQUAL "tgz"))
        #message("tar xvfz ${TMP_PATH} ${UncompressArguments}")
        execute_process(
          COMMAND sh -c "tar xvfz ${TMP_PATH} ${UncompressArguments}"
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          )
      elseif ("${TMP_EXTENSION}" STREQUAL "bz2")
        execute_process(
          COMMAND sh -c "tar xvfj ${TMP_PATH} ${UncompressArguments}"
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          )
      else()
        message(FATAL_ERROR "Unknown package format.")
      endif()
    else()
      message(FATAL_ERROR "Support your platform here")
    endif()

    if (NOT IS_DIRECTORY "${TargetDirectory}")
      message(FATAL_ERROR "The package was not uncompressed at the proper location. Check the CMake instructions.")
    endif()
  endif()
endmacro()

