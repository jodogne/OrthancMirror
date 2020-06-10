cmake_minimum_required(VERSION 2.8)

project(Orthanc)


#####################################################################
## Generic parameters of the Orthanc framework
#####################################################################

include(${CMAKE_SOURCE_DIR}/OrthancFramework/Resources/CMake/OrthancFrameworkParameters.cmake)

# Enable all the optional components of the Orthanc framework
set(ENABLE_CRYPTO_OPTIONS ON)
set(ENABLE_DCMTK ON)
set(ENABLE_DCMTK_NETWORKING ON)
set(ENABLE_DCMTK_TRANSCODING ON)
set(ENABLE_GOOGLE_TEST ON)
set(ENABLE_JPEG ON)
set(ENABLE_LOCALE ON)
set(ENABLE_LUA ON)
set(ENABLE_OPENSSL_ENGINES ON)  # OpenSSL engines are necessary for PKCS11
set(ENABLE_PNG ON)
set(ENABLE_PUGIXML ON)
set(ENABLE_SQLITE ON)
set(ENABLE_WEB_CLIENT ON)
set(ENABLE_WEB_SERVER ON)
set(ENABLE_ZLIB ON)


#####################################################################
## CMake parameters tunable at the command line to configure the
## plugins, the companion tools, and the unit tests
#####################################################################

# Parameters of the build
set(STANDALONE_BUILD ON CACHE BOOL "Standalone build (all the resources are embedded, necessary for releases)")
SET(BUILD_MODALITY_WORKLISTS ON CACHE BOOL "Whether to build the sample plugin to serve modality worklists")
SET(BUILD_RECOVER_COMPRESSED_FILE ON CACHE BOOL "Whether to build the companion tool to recover files compressed using Orthanc")
SET(BUILD_SERVE_FOLDERS ON CACHE BOOL "Whether to build the ServeFolders plugin")
SET(BUILD_CONNECTIVITY_CHECKS ON CACHE BOOL "Whether to build the ConnectivityChecks plugin")
SET(ENABLE_PLUGINS ON CACHE BOOL "Enable plugins")
SET(UNIT_TESTS_WITH_HTTP_CONNEXIONS ON CACHE BOOL "Allow unit tests to make HTTP requests")


#####################################################################
## Configuration of the Orthanc framework
#####################################################################

include(${CMAKE_SOURCE_DIR}/OrthancFramework/Resources/CMake/VisualStudioPrecompiledHeaders.cmake)
include(${CMAKE_SOURCE_DIR}/OrthancFramework/Resources/CMake/OrthancFrameworkConfiguration.cmake)


#####################################################################
## List of source files
#####################################################################

set(ORTHANC_SERVER_SOURCES
  OrthancServer/Sources/Database/Compatibility/DatabaseLookup.cpp
  OrthancServer/Sources/Database/Compatibility/ICreateInstance.cpp
  OrthancServer/Sources/Database/Compatibility/IGetChildrenMetadata.cpp
  OrthancServer/Sources/Database/Compatibility/ILookupResourceAndParent.cpp
  OrthancServer/Sources/Database/Compatibility/ILookupResources.cpp
  OrthancServer/Sources/Database/Compatibility/SetOfResources.cpp
  OrthancServer/Sources/Database/ResourcesContent.cpp
  OrthancServer/Sources/Database/SQLiteDatabaseWrapper.cpp
  OrthancServer/Sources/DicomInstanceOrigin.cpp
  OrthancServer/Sources/DicomInstanceToStore.cpp
  OrthancServer/Sources/EmbeddedResourceHttpHandler.cpp
  OrthancServer/Sources/ExportedResource.cpp
  OrthancServer/Sources/LuaScripting.cpp
  OrthancServer/Sources/OrthancConfiguration.cpp
  OrthancServer/Sources/OrthancFindRequestHandler.cpp
  OrthancServer/Sources/OrthancGetRequestHandler.cpp
  OrthancServer/Sources/OrthancHttpHandler.cpp
  OrthancServer/Sources/OrthancInitialization.cpp
  OrthancServer/Sources/OrthancMoveRequestHandler.cpp
  OrthancServer/Sources/OrthancRestApi/OrthancRestAnonymizeModify.cpp
  OrthancServer/Sources/OrthancRestApi/OrthancRestApi.cpp
  OrthancServer/Sources/OrthancRestApi/OrthancRestArchive.cpp
  OrthancServer/Sources/OrthancRestApi/OrthancRestChanges.cpp
  OrthancServer/Sources/OrthancRestApi/OrthancRestModalities.cpp
  OrthancServer/Sources/OrthancRestApi/OrthancRestResources.cpp
  OrthancServer/Sources/OrthancRestApi/OrthancRestSystem.cpp
  OrthancServer/Sources/QueryRetrieveHandler.cpp
  OrthancServer/Sources/Search/DatabaseConstraint.cpp
  OrthancServer/Sources/Search/DatabaseLookup.cpp
  OrthancServer/Sources/Search/DicomTagConstraint.cpp
  OrthancServer/Sources/Search/HierarchicalMatcher.cpp
  OrthancServer/Sources/Search/ISqlLookupFormatter.cpp
  OrthancServer/Sources/ServerContext.cpp
  OrthancServer/Sources/ServerEnumerations.cpp
  OrthancServer/Sources/ServerIndex.cpp
  OrthancServer/Sources/ServerJobs/ArchiveJob.cpp
  OrthancServer/Sources/ServerJobs/CleaningInstancesJob.cpp
  OrthancServer/Sources/ServerJobs/DicomModalityStoreJob.cpp
  OrthancServer/Sources/ServerJobs/DicomMoveScuJob.cpp
  OrthancServer/Sources/ServerJobs/LuaJobManager.cpp
  OrthancServer/Sources/ServerJobs/MergeStudyJob.cpp
  OrthancServer/Sources/ServerJobs/Operations/DeleteResourceOperation.cpp
  OrthancServer/Sources/ServerJobs/Operations/DicomInstanceOperationValue.cpp
  OrthancServer/Sources/ServerJobs/Operations/ModifyInstanceOperation.cpp
  OrthancServer/Sources/ServerJobs/Operations/StorePeerOperation.cpp
  OrthancServer/Sources/ServerJobs/Operations/StoreScuOperation.cpp
  OrthancServer/Sources/ServerJobs/Operations/SystemCallOperation.cpp
  OrthancServer/Sources/ServerJobs/OrthancJobUnserializer.cpp
  OrthancServer/Sources/ServerJobs/OrthancPeerStoreJob.cpp
  OrthancServer/Sources/ServerJobs/ResourceModificationJob.cpp
  OrthancServer/Sources/ServerJobs/SplitStudyJob.cpp
  OrthancServer/Sources/ServerJobs/StorageCommitmentScpJob.cpp
  OrthancServer/Sources/ServerToolbox.cpp
  OrthancServer/Sources/SliceOrdering.cpp
  OrthancServer/Sources/StorageCommitmentReports.cpp
  )


set(ORTHANC_FRAMEWORK_UNIT_TESTS
  OrthancFramework/UnitTestsSources/FrameworkTests.cpp
  OrthancFramework/UnitTestsSources/ImageProcessingTests.cpp
  OrthancFramework/UnitTestsSources/JpegLosslessTests.cpp
  OrthancFramework/UnitTestsSources/LoggingTests.cpp
  OrthancFramework/UnitTestsSources/RestApiTests.cpp
  OrthancFramework/UnitTestsSources/SQLiteChromiumTests.cpp
  OrthancFramework/UnitTestsSources/SQLiteTests.cpp
  OrthancFramework/UnitTestsSources/StreamTests.cpp
  OrthancFramework/UnitTestsSources/ToolboxTests.cpp
  OrthancFramework/UnitTestsSources/ZipTests.cpp
  )

set(ORTHANC_SERVER_UNIT_TESTS
  OrthancServer/UnitTestsSources/DatabaseLookupTests.cpp
  OrthancServer/UnitTestsSources/DicomMapTests.cpp
  OrthancServer/UnitTestsSources/FileStorageTests.cpp
  OrthancServer/UnitTestsSources/FromDcmtkTests.cpp
  OrthancServer/UnitTestsSources/ImageTests.cpp
  OrthancServer/UnitTestsSources/LuaTests.cpp
  OrthancServer/UnitTestsSources/MemoryCacheTests.cpp
  OrthancServer/UnitTestsSources/MultiThreadingTests.cpp
  OrthancServer/UnitTestsSources/ServerIndexTests.cpp
  OrthancServer/UnitTestsSources/UnitTestsMain.cpp
  OrthancServer/UnitTestsSources/VersionsTests.cpp
  )


if (ENABLE_PLUGINS)
  include_directories(${CMAKE_SOURCE_DIR}/OrthancServer/Plugins/Include)

  list(APPEND ORTHANC_SERVER_SOURCES
    OrthancServer/Plugins/Engine/OrthancPluginDatabase.cpp
    OrthancServer/Plugins/Engine/OrthancPlugins.cpp
    OrthancServer/Plugins/Engine/PluginsEnumerations.cpp
    OrthancServer/Plugins/Engine/PluginsErrorDictionary.cpp
    OrthancServer/Plugins/Engine/PluginsJob.cpp
    OrthancServer/Plugins/Engine/PluginsManager.cpp
    )

  list(APPEND ORTHANC_SERVER_UNIT_TESTS
    OrthancServer/UnitTestsSources/PluginsTests.cpp
    )
endif()


if (CMAKE_COMPILER_IS_GNUCXX
    AND NOT CMAKE_CROSSCOMPILING 
    AND DCMTK_STATIC_VERSION STREQUAL "3.6.0")
  # Add the "-pedantic" flag only on the Orthanc sources, and only if
  # cross-compiling DCMTK 3.6.0
  set(ORTHANC_ALL_SOURCES
    ${ORTHANC_CORE_SOURCES_INTERNAL}
    ${ORTHANC_DICOM_SOURCES_INTERNAL}
    ${ORTHANC_SERVER_SOURCES}
    ${ORTHANC_FRAMEWORK_UNIT_TESTS}
    ${ORTHANC_SERVER_UNIT_TESTS}
    ${ORTHANC_ROOT}/OrthancServer/Plugins/Samples/ModalityWorklists/Plugin.cpp
    ${ORTHANC_ROOT}/OrthancServer/Plugins/Samples/ServeFolders/Plugin.cpp
    ${ORTHANC_ROOT}/OrthancServer/Sources/EmbeddedResourceHttpHandler.cpp
    ${ORTHANC_ROOT}/OrthancServer/Sources/main.cpp
    )

  set_source_files_properties(${ORTHANC_ALL_SOURCES}
    PROPERTIES COMPILE_FLAGS -pedantic
    )
endif()


#####################################################################
## Autogeneration of files
#####################################################################

set(ORTHANC_EMBEDDED_FILES
  CONFIGURATION_SAMPLE         ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Resources/Configuration.json
  DICOM_CONFORMANCE_STATEMENT  ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Resources/DicomConformanceStatement.txt
  FONT_UBUNTU_MONO_BOLD_16     ${CMAKE_CURRENT_SOURCE_DIR}/OrthancFramework/Resources/Fonts/UbuntuMonoBold-16.json
  LUA_TOOLBOX                  ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Resources/Toolbox.lua
  PREPARE_DATABASE             ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Sources/Database/PrepareDatabase.sql
  UPGRADE_DATABASE_3_TO_4      ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Sources/Database/Upgrade3To4.sql
  UPGRADE_DATABASE_4_TO_5      ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Sources/Database/Upgrade4To5.sql

  INSTALL_TRACK_ATTACHMENTS_SIZE
  ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/Sources/Database/InstallTrackAttachmentsSize.sql
  )

if (STANDALONE_BUILD)
  # We embed all the resources in the binaries for standalone builds
  add_definitions(
    -DORTHANC_STANDALONE=1
    )

  list(APPEND ORTHANC_EMBEDDED_FILES
    ORTHANC_EXPLORER ${CMAKE_CURRENT_SOURCE_DIR}/OrthancServer/OrthancExplorer
    )
else()
  add_definitions(
    -DORTHANC_PATH=\"${CMAKE_SOURCE_DIR}\"
    -DORTHANC_STANDALONE=0
    )
endif()

EmbedResources(
  --namespace=Orthanc.ServerResources
  --target=OrthancServerResources
  --framework-path=${CMAKE_SOURCE_DIR}/OrthancFramework/Sources
  ${ORTHANC_EMBEDDED_FILES}
  )


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
    "PrecompiledHeadersServer.h" "OrthancServer/Sources/PrecompiledHeadersServer.cpp"
    ORTHANC_SERVER_SOURCES ORTHANC_SERVER_PCH)

  ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
    "PrecompiledHeadersUnitTests.h" "UnitTestsSources/PrecompiledHeadersUnitTests.cpp"
    ORTHANC_SERVER_UNIT_TESTS ORTHANC_UNIT_TESTS_PCH)
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

if (LIBICU_LIBRARIES)
  target_link_libraries(CoreLibrary ${LIBICU_LIBRARIES})
endif()


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
  OrthancServer/Sources/main.cpp
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
  ${ORTHANC_FRAMEWORK_UNIT_TESTS}
  ${ORTHANC_SERVER_UNIT_TESTS}
  ${BOOST_EXTENDED_SOURCES}
  )

target_link_libraries(UnitTests
  ServerLibrary
  CoreLibrary
  ${DCMTK_LIBRARIES}
  ${GOOGLE_TEST_LIBRARIES}
  )


#####################################################################
## Build a static library to share code between the plugins
#####################################################################

if (ENABLE_PLUGINS AND
    (BUILD_SERVE_FOLDERS OR BUILD_MODALITY_WORKLISTS))
  add_library(ThirdPartyPlugins STATIC
    ${BOOST_SOURCES}
    ${JSONCPP_SOURCES}
    ${LIBICONV_SOURCES}
    ${LIBICU_SOURCES}
    OrthancServer/Plugins/Samples/Common/OrthancPluginCppWrapper.cpp
    )

  if (LIBICU_LIBRARIES)
    target_link_libraries(ThirdPartyPlugins ${LIBICU_LIBRARIES})
  endif()

  # Add the "-fPIC" option as this static library must be embedded
  # inside shared libraries (important on UNIX)
  set_property(
    TARGET ThirdPartyPlugins
    PROPERTY POSITION_INDEPENDENT_CODE ON
    )
endif()


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
    ${ORTHANC_ROOT}/OrthancServer/Plugins/Samples/ServeFolders/Plugin.cpp
    ${SERVE_FOLDERS_RESOURCES}
    )

  target_link_libraries(ServeFolders ThirdPartyPlugins)

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
    ${ORTHANC_ROOT}/OrthancServer/Plugins/Samples/ModalityWorklists/Plugin.cpp
    ${MODALITY_WORKLISTS_RESOURCES}
    )

  target_link_libraries(ModalityWorklists ThirdPartyPlugins)

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
## Build the "ConnectivityChecks" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_CONNECTIVITY_CHECKS)
  include(ExternalProject)

  set(Flags)

  if (CMAKE_TOOLCHAIN_FILE)
    # Take absolute path to the toolchain
    get_filename_component(TMP ${CMAKE_TOOLCHAIN_FILE} REALPATH BASE ${CMAKE_SOURCE_DIR})
    list(APPEND Flags -DCMAKE_TOOLCHAIN_FILE=${TMP})
  endif()

  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    list(APPEND Flags
      -DLSB_CC=${CMAKE_LSB_CC}
      -DLSB_CXX=${CMAKE_LSB_CXX}
      )
  endif()

  externalproject_add(ConnectivityChecks
    SOURCE_DIR "${ORTHANC_ROOT}/OrthancServer/Plugins/Samples/ConnectivityChecks"

    # We explicitly provide a build directory, in order to avoid paths
    # that are too long on our Visual Studio 2008 CIS
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ConnectivityChecks-build"

    CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
    -DPLUGIN_VERSION=${ORTHANC_VERSION}
    -DSTATIC_BUILD=${STATIC_BUILD}
    -DALLOW_DOWNLOADS=${ALLOW_DOWNLOADS}
    -DUSE_LEGACY_JSONCPP=${USE_LEGACY_JSONCPP}
    ${Flags}
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    if (MSVC)
      set(Prefix "")
    else()
      set(Prefix "lib")  # MinGW
    endif()

    install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/${Prefix}ConnectivityChecks.dll
      DESTINATION "lib")
  else()
    list(GET CMAKE_FIND_LIBRARY_PREFIXES 0 Prefix)
    list(GET CMAKE_FIND_LIBRARY_SUFFIXES 0 Suffix)
    install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/${Prefix}ConnectivityChecks${Suffix}
      ${CMAKE_CURRENT_BINARY_DIR}/${Prefix}ConnectivityChecks${Suffix}.${ORTHANC_VERSION}
      DESTINATION "share/orthanc/plugins")
  endif()
endif()



#####################################################################
## Build the companion tool to recover files compressed using Orthanc
#####################################################################

if (BUILD_RECOVER_COMPRESSED_FILE)
  set(RECOVER_COMPRESSED_SOURCES
    ${ORTHANC_ROOT}/OrthancServer/Resources/Samples/Tools/RecoverCompressedFile.cpp
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
    ${CMAKE_SOURCE_DIR}/OrthancServer/Resources/Orthanc.doxygen
    ${CMAKE_CURRENT_BINARY_DIR}/Orthanc.doxygen
    @ONLY)

  configure_file(
    ${CMAKE_SOURCE_DIR}/OrthancServer/Resources/OrthancPlugin.doxygen
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
    OrthancServer/Plugins/Include/orthanc/OrthancCPlugin.h 
    OrthancServer/Plugins/Include/orthanc/OrthancCDatabasePlugin.h 
    DESTINATION include/orthanc
    )
endif()



#####################################################################
## Prepare the "uninstall" target
## http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F
#####################################################################

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/OrthancFramework/Resources/CMake/Uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
