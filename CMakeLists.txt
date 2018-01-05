cmake_minimum_required(VERSION 2.8)

project(Orthanc)


#####################################################################
## Generic parameters of the Orthanc framework
#####################################################################

include(${CMAKE_SOURCE_DIR}/Resources/CMake/OrthancFrameworkParameters.cmake)

# Enable all the optional components of the Orthanc framework
set(ENABLE_CRYPTO_OPTIONS ON)
set(ENABLE_DCMTK ON)
set(ENABLE_DCMTK_NETWORKING ON)
set(ENABLE_GOOGLE_TEST ON)
set(ENABLE_JPEG ON)
set(ENABLE_LOCALE ON)
set(ENABLE_LUA ON)
set(ENABLE_PNG ON)
set(ENABLE_PUGIXML ON)
set(ENABLE_SQLITE ON)
set(ENABLE_WEB_CLIENT ON)
set(ENABLE_WEB_SERVER ON)

set(HAS_EMBEDDED_RESOURCES ON)


#####################################################################
## CMake parameters tunable at the command line to configure the
## plugins, the companion tools, and the unit tests
#####################################################################

# Parameters of the build
SET(BUILD_MODALITY_WORKLISTS ON CACHE BOOL "Whether to build the sample plugin to serve modality worklists")
SET(BUILD_RECOVER_COMPRESSED_FILE ON CACHE BOOL "Whether to build the companion tool to recover files compressed using Orthanc")
SET(BUILD_SERVE_FOLDERS ON CACHE BOOL "Whether to build the ServeFolders plugin")
SET(ENABLE_PLUGINS ON CACHE BOOL "Enable plugins")
SET(UNIT_TESTS_WITH_HTTP_CONNEXIONS ON CACHE BOOL "Allow unit tests to make HTTP requests")


#####################################################################
## Configuration of the Orthanc framework
#####################################################################

include(${CMAKE_SOURCE_DIR}/Resources/CMake/VisualStudioPrecompiledHeaders.cmake)
include(${CMAKE_SOURCE_DIR}/Resources/CMake/OrthancFrameworkConfiguration.cmake)

include_directories(${ORTHANC_ROOT})


#####################################################################
## List of source files
#####################################################################

set(ORTHANC_SERVER_SOURCES
  OrthancServer/DatabaseWrapper.cpp
  OrthancServer/DatabaseWrapperBase.cpp
  OrthancServer/DicomInstanceToStore.cpp
  OrthancServer/ExportedResource.cpp
  OrthancServer/LuaScripting.cpp
  OrthancServer/OrthancFindRequestHandler.cpp
  OrthancServer/OrthancHttpHandler.cpp
  OrthancServer/OrthancInitialization.cpp
  OrthancServer/OrthancMoveRequestHandler.cpp
  OrthancServer/OrthancRestApi/OrthancRestAnonymizeModify.cpp
  OrthancServer/OrthancRestApi/OrthancRestApi.cpp
  OrthancServer/OrthancRestApi/OrthancRestArchive.cpp
  OrthancServer/OrthancRestApi/OrthancRestChanges.cpp
  OrthancServer/OrthancRestApi/OrthancRestModalities.cpp
  OrthancServer/OrthancRestApi/OrthancRestResources.cpp
  OrthancServer/OrthancRestApi/OrthancRestSystem.cpp
  OrthancServer/QueryRetrieveHandler.cpp
  OrthancServer/Scheduler/CallSystemCommand.cpp
  OrthancServer/Scheduler/DeleteInstanceCommand.cpp
  OrthancServer/Scheduler/ModifyInstanceCommand.cpp
  OrthancServer/Scheduler/ServerCommandInstance.cpp
  OrthancServer/Scheduler/ServerJob.cpp
  OrthancServer/Scheduler/ServerScheduler.cpp
  OrthancServer/Scheduler/StorePeerCommand.cpp
  OrthancServer/Scheduler/StoreScuCommand.cpp
  OrthancServer/Search/HierarchicalMatcher.cpp
  OrthancServer/Search/IFindConstraint.cpp
  OrthancServer/Search/ListConstraint.cpp
  OrthancServer/Search/LookupIdentifierQuery.cpp
  OrthancServer/Search/LookupResource.cpp
  OrthancServer/Search/RangeConstraint.cpp
  OrthancServer/Search/SetOfResources.cpp
  OrthancServer/Search/ValueConstraint.cpp
  OrthancServer/Search/WildcardConstraint.cpp
  OrthancServer/ServerContext.cpp
  OrthancServer/ServerEnumerations.cpp
  OrthancServer/ServerIndex.cpp
  OrthancServer/ServerToolbox.cpp
  OrthancServer/SliceOrdering.cpp
  )


set(ORTHANC_UNIT_TESTS_SOURCES
  UnitTestsSources/DicomMapTests.cpp
  UnitTestsSources/FileStorageTests.cpp
  UnitTestsSources/FromDcmtkTests.cpp
  UnitTestsSources/MemoryCacheTests.cpp
  UnitTestsSources/ImageTests.cpp
  UnitTestsSources/RestApiTests.cpp
  UnitTestsSources/SQLiteTests.cpp
  UnitTestsSources/SQLiteChromiumTests.cpp
  UnitTestsSources/ServerIndexTests.cpp
  UnitTestsSources/VersionsTests.cpp
  UnitTestsSources/ZipTests.cpp
  UnitTestsSources/LuaTests.cpp
  UnitTestsSources/MultiThreadingTests.cpp
  UnitTestsSources/UnitTestsMain.cpp
  UnitTestsSources/ImageProcessingTests.cpp
  UnitTestsSources/JpegLosslessTests.cpp
  UnitTestsSources/StreamTests.cpp
  )


if (ENABLE_PLUGINS)
  list(APPEND ORTHANC_SERVER_SOURCES
    Plugins/Engine/OrthancPluginDatabase.cpp
    Plugins/Engine/OrthancPlugins.cpp
    Plugins/Engine/PluginsEnumerations.cpp
    Plugins/Engine/PluginsErrorDictionary.cpp
    Plugins/Engine/PluginsManager.cpp
    )

  list(APPEND ORTHANC_UNIT_TESTS_SOURCES
    UnitTestsSources/PluginsTests.cpp
    )
endif()


if (CMAKE_COMPILER_IS_GNUCXX
    AND NOT CMAKE_CROSSCOMPILING 
    AND USE_DCMTK_360)
  # Add the "-pedantic" flag only on the Orthanc sources, and only if
  # cross-compiling DCMTK 3.6.0
  set(ORTHANC_ALL_SOURCES
    ${ORTHANC_CORE_SOURCES_INTERNAL}
    ${ORTHANC_DICOM_SOURCES_INTERNAL}
    ${ORTHANC_SERVER_SOURCES}
    ${ORTHANC_UNIT_TESTS_SOURCES}
    Plugins/Samples/ServeFolders/Plugin.cpp
    Plugins/Samples/ModalityWorklists/Plugin.cpp
    OrthancServer/main.cpp
    )

  set_source_files_properties(${ORTHANC_ALL_SOURCES}
    PROPERTIES COMPILE_FLAGS -pedantic
    )
endif()


#####################################################################
## Autogeneration of files
#####################################################################

set(ORTHANC_EMBEDDED_FILES
  PREPARE_DATABASE            ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/PrepareDatabase.sql
  UPGRADE_DATABASE_3_TO_4     ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Upgrade3To4.sql
  UPGRADE_DATABASE_4_TO_5     ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Upgrade4To5.sql
  CONFIGURATION_SAMPLE        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Configuration.json
  DICOM_CONFORMANCE_STATEMENT ${CMAKE_CURRENT_SOURCE_DIR}/Resources/DicomConformanceStatement.txt
  LUA_TOOLBOX                 ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Toolbox.lua
  FONT_UBUNTU_MONO_BOLD_16    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/UbuntuMonoBold-16.json
  )

if (STANDALONE_BUILD)
  # We embed all the resources in the binaries for standalone builds
  add_definitions(-DORTHANC_STANDALONE=1)
  EmbedResources(
    ${ORTHANC_EMBEDDED_FILES}
    ORTHANC_EXPLORER ${CMAKE_CURRENT_SOURCE_DIR}/OrthancExplorer
    ${DCMTK_DICTIONARIES}
    )
else()
  add_definitions(
    -DORTHANC_STANDALONE=0
    -DORTHANC_PATH=\"${CMAKE_SOURCE_DIR}\"
    )
  EmbedResources(
    ${ORTHANC_EMBEDDED_FILES}
    )
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  execute_process(
    COMMAND 
    ${PYTHON_EXECUTABLE} ${ORTHANC_ROOT}/Resources/WindowsResources.py
    ${ORTHANC_VERSION} Orthanc Orthanc.exe "Lightweight, RESTful DICOM server for medical imaging"
    ERROR_VARIABLE Failure
    OUTPUT_FILE ${AUTOGENERATED_DIR}/Orthanc.rc
    )

  if (Failure)
    message(FATAL_ERROR "Error while computing the version information: ${Failure}")
  endif()

  list(APPEND ORTHANC_RESOURCES ${AUTOGENERATED_DIR}/Orthanc.rc)
endif()



#####################################################################
## Configuration of the C/C++ macros
#####################################################################

if (ENABLE_PLUGINS)
  add_definitions(-DORTHANC_ENABLE_PLUGINS=1)
else()
  add_definitions(-DORTHANC_ENABLE_PLUGINS=0)
endif()


if (UNIT_TESTS_WITH_HTTP_CONNEXIONS)
  add_definitions(-DUNIT_TESTS_WITH_HTTP_CONNEXIONS=1)
else()
  add_definitions(-DUNIT_TESTS_WITH_HTTP_CONNEXIONS=0)
endif()


include_directories(${CMAKE_SOURCE_DIR}/Plugins/Include)

add_definitions(
  -DORTHANC_BUILD_UNIT_TESTS=1
  
  # Macros for the plugins
  -DHAS_ORTHANC_EXCEPTION=0
  -DMODALITY_WORKLISTS_VERSION="${ORTHANC_VERSION}"
  -DSERVE_FOLDERS_VERSION="${ORTHANC_VERSION}"
  )


# Setup precompiled headers for Microsoft Visual Studio

# WARNING: There must be NO MORE "add_definitions()", "include()" or
# "include_directories()" below, otherwise the generated precompiled
# headers might get broken!

if (MSVC)
  add_definitions(-DORTHANC_USE_PRECOMPILED_HEADERS=1)

  set(TMP
    ${ORTHANC_CORE_SOURCES_INTERNAL}
    ${ORTHANC_DICOM_SOURCES_INTERNAL}
    )
  
  ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
    "PrecompiledHeaders.h" "Core/PrecompiledHeaders.cpp"
    TMP ORTHANC_CORE_PCH)

  ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
    "PrecompiledHeadersServer.h" "OrthancServer/PrecompiledHeadersServer.cpp"
    ORTHANC_SERVER_SOURCES ORTHANC_SERVER_PCH)

  ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
    "PrecompiledHeadersUnitTests.h" "UnitTestsSources/PrecompiledHeadersUnitTests.cpp"
    ORTHANC_UNIT_TESTS_SOURCES ORTHANC_UNIT_TESTS_PCH)
endif()



#####################################################################
## Build the core of Orthanc
#####################################################################

# "CoreLibrary" contains all the third-party dependencies and the
# content of the "Core" folder
add_library(CoreLibrary
  STATIC
  ${ORTHANC_CORE_PCH}
  ${ORTHANC_CORE_SOURCES}
  ${ORTHANC_DICOM_SOURCES}
  ${AUTOGENERATED_SOURCES}
  )  


#####################################################################
## Build the Orthanc server
#####################################################################

add_library(ServerLibrary
  STATIC
  ${ORTHANC_SERVER_PCH}
  ${ORTHANC_SERVER_SOURCES}
  )

# Ensure autogenerated code is built before building ServerLibrary
add_dependencies(ServerLibrary CoreLibrary)

add_executable(Orthanc
  OrthancServer/main.cpp
  ${ORTHANC_RESOURCES}
  )

target_link_libraries(Orthanc ServerLibrary CoreLibrary ${DCMTK_LIBRARIES})

install(
  TARGETS Orthanc
  RUNTIME DESTINATION sbin
  )


#####################################################################
## Build the unit tests
#####################################################################

add_executable(UnitTests
  ${GOOGLE_TEST_SOURCES}
  ${ORTHANC_UNIT_TESTS_PCH}
  ${ORTHANC_UNIT_TESTS_SOURCES}
  )

target_link_libraries(UnitTests
  ServerLibrary
  CoreLibrary
  ${DCMTK_LIBRARIES}
  ${GOOGLE_TEST_LIBRARIES}
  )


#####################################################################
## Build the "ServeFolders" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_SERVE_FOLDERS)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${ORTHANC_ROOT}/Resources/WindowsResources.py
      ${ORTHANC_VERSION} ServeFolders ServeFolders.dll "Orthanc plugin to serve additional folders"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/ServeFolders.rc
      )

    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()

    list(APPEND SERVE_FOLDERS_RESOURCES ${AUTOGENERATED_DIR}/ServeFolders.rc)
  endif()  

  add_library(ServeFolders SHARED 
    ${BOOST_SOURCES}
    ${JSONCPP_SOURCES}
    ${LIBICONV_SOURCES}
    Plugins/Samples/ServeFolders/Plugin.cpp
    Plugins/Samples/Common/OrthancPluginCppWrapper.cpp
    ${SERVE_FOLDERS_RESOURCES}
    )

  set_target_properties(
    ServeFolders PROPERTIES 
    VERSION ${ORTHANC_VERSION} 
    SOVERSION ${ORTHANC_VERSION}
    )

  install(
    TARGETS ServeFolders
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()



#####################################################################
## Build the "ModalityWorklists" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_MODALITY_WORKLISTS)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${ORTHANC_ROOT}/Resources/WindowsResources.py
      ${ORTHANC_VERSION} ModalityWorklists ModalityWorklists.dll "Sample Orthanc plugin to serve modality worklists"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/ModalityWorklists.rc
      )

    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()

    list(APPEND MODALITY_WORKLISTS_RESOURCES ${AUTOGENERATED_DIR}/ModalityWorklists.rc)
  endif()

  add_library(ModalityWorklists SHARED 
    ${BOOST_SOURCES}
    ${JSONCPP_SOURCES}
    ${LIBICONV_SOURCES}
    Plugins/Samples/Common/OrthancPluginCppWrapper.cpp
    Plugins/Samples/ModalityWorklists/Plugin.cpp
    ${MODALITY_WORKLISTS_RESOURCES}
    )

  set_target_properties(
    ModalityWorklists PROPERTIES 
    VERSION ${ORTHANC_VERSION} 
    SOVERSION ${ORTHANC_VERSION}
    )

  install(
    TARGETS ModalityWorklists
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()



#####################################################################
## Build the companion tool to recover files compressed using Orthanc
#####################################################################

if (BUILD_RECOVER_COMPRESSED_FILE)
  set(RECOVER_COMPRESSED_SOURCES
    Resources/Samples/Tools/RecoverCompressedFile.cpp
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${ORTHANC_ROOT}/Resources/WindowsResources.py
      ${ORTHANC_VERSION} OrthancRecoverCompressedFile OrthancRecoverCompressedFile.exe
      "Lightweight, RESTful DICOM server for medical imaging"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/OrthancRecoverCompressedFile.rc
      )

    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()

    list(APPEND RECOVER_COMPRESSED_SOURCES
      ${AUTOGENERATED_DIR}/OrthancRecoverCompressedFile.rc
      )
  endif()

  add_executable(OrthancRecoverCompressedFile ${RECOVER_COMPRESSED_SOURCES})

  target_link_libraries(OrthancRecoverCompressedFile CoreLibrary)

  install(
    TARGETS OrthancRecoverCompressedFile
    RUNTIME DESTINATION bin
    )
endif()



#####################################################################
## Generate the documentation if Doxygen is present
#####################################################################

find_package(Doxygen)
if (DOXYGEN_FOUND)
  configure_file(
    ${CMAKE_SOURCE_DIR}/Resources/Orthanc.doxygen
    ${CMAKE_CURRENT_BINARY_DIR}/Orthanc.doxygen
    @ONLY)

  configure_file(
    ${CMAKE_SOURCE_DIR}/Resources/OrthancPlugin.doxygen
    ${CMAKE_CURRENT_BINARY_DIR}/OrthancPlugin.doxygen
    @ONLY)

  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Orthanc.doxygen
    COMMENT "Generating internal documentation with Doxygen" VERBATIM
    )

  add_custom_command(TARGET Orthanc
    POST_BUILD
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/OrthancPlugin.doxygen
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating plugin documentation with Doxygen" VERBATIM
    )

  install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/OrthancPluginDocumentation/doc/
    DESTINATION share/doc/orthanc/OrthancPlugin
    )
else()
  message("Doxygen not found. The documentation will not be built.")
endif()



#####################################################################
## Install the plugin SDK
#####################################################################

if (ENABLE_PLUGINS)
  install(
    FILES
    Plugins/Include/orthanc/OrthancCPlugin.h 
    Plugins/Include/orthanc/OrthancCDatabasePlugin.h 
    Plugins/Include/orthanc/OrthancCppDatabasePlugin.h 
    DESTINATION include/orthanc
    )
endif()



#####################################################################
## Prepare the "uninstall" target
## http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F
#####################################################################

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/CMake/Uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
