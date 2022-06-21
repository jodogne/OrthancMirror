# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2022 Osimis S.A., Belgium
# Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0058 NEW)

project(Orthanc)


#####################################################################
## Generic parameters of the Orthanc framework
#####################################################################

include(${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/CMake/OrthancFrameworkParameters.cmake)

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
SET(BUILD_HOUSEKEEPER ON CACHE BOOL "Whether to build the Housekeeper plugin")
SET(BUILD_DELAYED_DELETION ON CACHE BOOL "Whether to build the DelayedDeletion plugin")
SET(ENABLE_PLUGINS ON CACHE BOOL "Enable plugins")
SET(UNIT_TESTS_WITH_HTTP_CONNEXIONS ON CACHE BOOL "Allow unit tests to make HTTP requests")


#####################################################################
## Configuration of the Orthanc framework
#####################################################################

include(${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/CMake/VisualStudioPrecompiledHeaders.cmake)
include(${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/CMake/OrthancFrameworkConfiguration.cmake)

# To export the proper symbols in the sample plugins
include(${CMAKE_SOURCE_DIR}/Plugins/Samples/Common/OrthancPluginsExports.cmake)


#####################################################################
## List of source files
#####################################################################

set(ORTHANC_SERVER_SOURCES
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/DatabaseLookup.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/ICreateInstance.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/IGetChildrenMetadata.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/ILookupResourceAndParent.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/ILookupResources.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/Compatibility/SetOfResources.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/ResourcesContent.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/SQLiteDatabaseWrapper.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/StatelessDatabaseOperations.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Database/VoidDatabaseListener.cpp
  ${CMAKE_SOURCE_DIR}/Sources/DicomInstanceOrigin.cpp
  ${CMAKE_SOURCE_DIR}/Sources/DicomInstanceToStore.cpp
  ${CMAKE_SOURCE_DIR}/Sources/EmbeddedResourceHttpHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ExportedResource.cpp
  ${CMAKE_SOURCE_DIR}/Sources/LuaScripting.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancConfiguration.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancFindRequestHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancGetRequestHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancHttpHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancInitialization.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancMoveRequestHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestAnonymizeModify.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestApi.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestArchive.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestChanges.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestModalities.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestResources.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancRestApi/OrthancRestSystem.cpp
  ${CMAKE_SOURCE_DIR}/Sources/OrthancWebDav.cpp
  ${CMAKE_SOURCE_DIR}/Sources/QueryRetrieveHandler.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/DatabaseConstraint.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/DatabaseLookup.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/DicomTagConstraint.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/HierarchicalMatcher.cpp
  ${CMAKE_SOURCE_DIR}/Sources/Search/ISqlLookupFormatter.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerContext.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerEnumerations.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerIndex.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/ArchiveJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/CleaningInstancesJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/DicomModalityStoreJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/DicomMoveScuJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/LuaJobManager.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/MergeStudyJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/DeleteResourceOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/DicomInstanceOperationValue.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/ModifyInstanceOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/StorePeerOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/StoreScuOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/Operations/SystemCallOperation.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/OrthancJobUnserializer.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/OrthancPeerStoreJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/ResourceModificationJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/SplitStudyJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerJobs/StorageCommitmentScpJob.cpp
  ${CMAKE_SOURCE_DIR}/Sources/ServerToolbox.cpp
  ${CMAKE_SOURCE_DIR}/Sources/SliceOrdering.cpp
  ${CMAKE_SOURCE_DIR}/Sources/StorageCommitmentReports.cpp
  )


set(ORTHANC_FRAMEWORK_UNIT_TESTS
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/DicomMapTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/FileStorageTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/FrameworkTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/FromDcmtkTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/ImageProcessingTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/ImageTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/JobsTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/JpegLosslessTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/LoggingTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/LuaTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/MemoryCacheTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/RestApiTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/SQLiteChromiumTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/SQLiteTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/StreamTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/ToolboxTests.cpp
  ${CMAKE_SOURCE_DIR}/../OrthancFramework/UnitTestsSources/ZipTests.cpp
  )

set(ORTHANC_SERVER_UNIT_TESTS
  ${CMAKE_SOURCE_DIR}/UnitTestsSources/DatabaseLookupTests.cpp
  ${CMAKE_SOURCE_DIR}/UnitTestsSources/LuaServerTests.cpp
  ${CMAKE_SOURCE_DIR}/UnitTestsSources/PluginsTests.cpp
  ${CMAKE_SOURCE_DIR}/UnitTestsSources/ServerIndexTests.cpp
  ${CMAKE_SOURCE_DIR}/UnitTestsSources/ServerJobsTests.cpp
  ${CMAKE_SOURCE_DIR}/UnitTestsSources/SizeOfTests.cpp
  ${CMAKE_SOURCE_DIR}/UnitTestsSources/UnitTestsMain.cpp
  ${CMAKE_SOURCE_DIR}/UnitTestsSources/VersionsTests.cpp
  )


if (ENABLE_PLUGINS)
  include_directories(${CMAKE_SOURCE_DIR}/Plugins/Include)

  list(APPEND ORTHANC_SERVER_SOURCES
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/OrthancPluginDatabase.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/OrthancPluginDatabaseV3.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/OrthancPlugins.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginsEnumerations.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginsErrorDictionary.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginsJob.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Engine/PluginsManager.cpp
    )

  list(APPEND ORTHANC_SERVER_UNIT_TESTS
    ${CMAKE_SOURCE_DIR}/UnitTestsSources/PluginsTests.cpp
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
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ModalityWorklists/Plugin.cpp
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ServeFolders/Plugin.cpp
    ${CMAKE_SOURCE_DIR}/Sources/EmbeddedResourceHttpHandler.cpp
    ${CMAKE_SOURCE_DIR}/Sources/main.cpp
    )

  set_source_files_properties(${ORTHANC_ALL_SOURCES}
    PROPERTIES COMPILE_FLAGS -pedantic
    )
endif()


#####################################################################
## Autogeneration of files
#####################################################################

set(ORTHANC_EMBEDDED_FILES
  CONFIGURATION_SAMPLE         ${CMAKE_SOURCE_DIR}/Resources/Configuration.json
  DICOM_CONFORMANCE_STATEMENT  ${CMAKE_SOURCE_DIR}/Resources/DicomConformanceStatement.txt
  FONT_UBUNTU_MONO_BOLD_16     ${CMAKE_SOURCE_DIR}/Resources/Fonts/UbuntuMonoBold-16.json
  LUA_TOOLBOX                  ${CMAKE_SOURCE_DIR}/Resources/Toolbox.lua
  PREPARE_DATABASE             ${CMAKE_SOURCE_DIR}/Sources/Database/PrepareDatabase.sql
  UPGRADE_DATABASE_3_TO_4      ${CMAKE_SOURCE_DIR}/Sources/Database/Upgrade3To4.sql
  UPGRADE_DATABASE_4_TO_5      ${CMAKE_SOURCE_DIR}/Sources/Database/Upgrade4To5.sql

  INSTALL_TRACK_ATTACHMENTS_SIZE
  ${CMAKE_SOURCE_DIR}/Sources/Database/InstallTrackAttachmentsSize.sql
  )

if (STANDALONE_BUILD)
  # We embed all the resources in the binaries for standalone builds
  add_definitions(
    -DORTHANC_STANDALONE=1
    )

  list(APPEND ORTHANC_EMBEDDED_FILES
    ORTHANC_EXPLORER ${CMAKE_SOURCE_DIR}/OrthancExplorer
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
  --framework-path=${CMAKE_SOURCE_DIR}/../OrthancFramework/Sources
  ${ORTHANC_EMBEDDED_FILES}
  )


if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  execute_process(
    COMMAND 
    ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
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

check_symbol_exists(mallopt "malloc.h" HAVE_MALLOPT)

if (HAVE_MALLOPT)
  add_definitions(-DHAVE_MALLOPT=1)
else()
  add_definitions(-DHAVE_MALLOPT=0)
endif()


if (STATIC_BUILD)
  add_definitions(-DORTHANC_STATIC=1)
else()
  add_definitions(-DORTHANC_STATIC=0)
endif()


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
  -DORTHANC_BUILDING_SERVER_LIBRARY=1
  
  # Macros for the plugins
  -DHAS_ORTHANC_EXCEPTION=0
  -DMODALITY_WORKLISTS_VERSION="${ORTHANC_VERSION}"
  -DSERVE_FOLDERS_VERSION="${ORTHANC_VERSION}"
  -DHOUSEKEEPER_VERSION="${ORTHANC_VERSION}"
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
    "PrecompiledHeaders.h" "${CMAKE_SOURCE_DIR}/../OrthancFramework/Sources/PrecompiledHeaders.cpp"
    TMP ORTHANC_CORE_PCH)

  ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
    "PrecompiledHeadersServer.h" "${CMAKE_SOURCE_DIR}/Sources/PrecompiledHeadersServer.cpp"
    ORTHANC_SERVER_SOURCES ORTHANC_SERVER_PCH)

  ADD_VISUAL_STUDIO_PRECOMPILED_HEADERS(
    "PrecompiledHeadersUnitTests.h" "${CMAKE_SOURCE_DIR}/UnitTestsSources/PrecompiledHeadersUnitTests.cpp"
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
  ${CMAKE_SOURCE_DIR}/Sources/main.cpp
  ${ORTHANC_RESOURCES}
  )

target_link_libraries(Orthanc ServerLibrary CoreLibrary ${DCMTK_LIBRARIES})

if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
  # The link flag below hides all the global functions so that a Linux
  # Standard Base (LSB) build of Orthanc can load plugins that are not
  # built using LSB (new in Orthanc 1.9.7)
  set_property(
    TARGET Orthanc
    PROPERTY LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/Resources/VersionScriptOrthanc.map"
    )
endif()

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
    (BUILD_SERVE_FOLDERS OR BUILD_MODALITY_WORKLISTS OR BUILD_HOUSEKEEPER))
  add_library(ThirdPartyPlugins STATIC
    ${BOOST_SOURCES}
    ${JSONCPP_SOURCES}
    ${LIBICONV_SOURCES}
    ${LIBICU_SOURCES}
    Plugins/Samples/Common/OrthancPluginCppWrapper.cpp
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
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
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
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ServeFolders/Plugin.cpp
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
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
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
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/ModalityWorklists/Plugin.cpp
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


if (ENABLE_PLUGINS AND (BUILD_DELAYED_DELETION OR BUILD_CONNECTIVITY_CHECKS))
  include(ExternalProject)

endif()


#####################################################################
## Build the "ConnectivityChecks" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_CONNECTIVITY_CHECKS)

  set(ConnectivityChecksFlags)

  if (CMAKE_TOOLCHAIN_FILE)
    # Take absolute path to the toolchain
    get_filename_component(TMP ${CMAKE_TOOLCHAIN_FILE} REALPATH BASE ${CMAKE_SOURCE_DIR})
    list(APPEND ConnectivityChecksFlags -DCMAKE_TOOLCHAIN_FILE=${TMP})
  endif()

  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    list(APPEND ConnectivityChecksFlags
      -DLSB_CC=${CMAKE_LSB_CC}
      -DLSB_CXX=${CMAKE_LSB_CXX}
      )
  endif()

  externalproject_add(ConnectivityChecks
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/Plugins/Samples/ConnectivityChecks"

    # We explicitly provide a build directory, in order to avoid paths
    # that are too long on our Visual Studio 2008 CIS
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ConnectivityChecks-build"

    # this helps triggering build when changing the external project
    BUILD_ALWAYS 1

    CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
    -DPLUGIN_VERSION=${ORTHANC_VERSION}
    -DSTATIC_BUILD=${STATIC_BUILD}
    -DALLOW_DOWNLOADS=${ALLOW_DOWNLOADS}
    -DUSE_SYSTEM_BOOST=${USE_SYSTEM_BOOST}
    -DUSE_LEGACY_JSONCPP=${USE_LEGACY_JSONCPP}
    ${ConnectivityChecksFlags}

    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
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
## Build the "DelayedDeletion" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_DELAYED_DELETION)

  set(DelayedDeletionFlags)

  if (CMAKE_TOOLCHAIN_FILE)
    # Take absolute path to the toolchain
    get_filename_component(TMP ${CMAKE_TOOLCHAIN_FILE} REALPATH BASE ${CMAKE_SOURCE_DIR})
    list(APPEND DelayedDeletionFlags -DCMAKE_TOOLCHAIN_FILE=${TMP})
  endif()

  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    list(APPEND DelayedDeletionFlags
      -DLSB_CC=${CMAKE_LSB_CC}
      -DLSB_CXX=${CMAKE_LSB_CXX}
      )
  endif()

  externalproject_add(DelayedDeletion
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/Plugins/Samples/DelayedDeletion"

    # We explicitly provide a build directory, in order to avoid paths
    # that are too long on our Visual Studio 2008 CIS
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/DelayedDeletion-build"

    # this helps triggering build when changing the external project
    BUILD_ALWAYS 1

    CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
    -DPLUGIN_VERSION=${ORTHANC_VERSION}
    -DSTATIC_BUILD=${STATIC_BUILD}
    -DALLOW_DOWNLOADS=${ALLOW_DOWNLOADS}
    -DUSE_SYSTEM_BOOST=${USE_SYSTEM_BOOST}
    -DUSE_LEGACY_JSONCPP=${USE_LEGACY_JSONCPP}
    ${DelayedDeletionFlags}

    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    if (MSVC)
      set(Prefix "")
    else()
      set(Prefix "lib")  # MinGW
    endif()

    install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/${Prefix}DelayedDeletion.dll
      DESTINATION "lib")
  else()
    list(GET CMAKE_FIND_LIBRARY_PREFIXES 0 Prefix)
    list(GET CMAKE_FIND_LIBRARY_SUFFIXES 0 Suffix)
    install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/${Prefix}DelayedDeletion${Suffix}
      ${CMAKE_CURRENT_BINARY_DIR}/${Prefix}DelayedDeletion${Suffix}.${ORTHANC_VERSION}
      DESTINATION "share/orthanc/plugins")
  endif()
endif()


#####################################################################
## Build the "Housekeeper" plugin
#####################################################################

if (ENABLE_PLUGINS AND BUILD_HOUSEKEEPER)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
      ${ORTHANC_VERSION} Housekeeper Housekeeper.dll "Sample Orthanc plugin to optimize/clean the DB/Storage"
      ERROR_VARIABLE Failure
      OUTPUT_FILE ${AUTOGENERATED_DIR}/Housekeeper.rc
      )
    
    if (Failure)
      message(FATAL_ERROR "Error while computing the version information: ${Failure}")
    endif()
    
    list(APPEND HOUSEKEEPER_RESOURCES ${AUTOGENERATED_DIR}/Housekeeper.rc)
  endif()
  
  add_library(Housekeeper SHARED 
    ${CMAKE_SOURCE_DIR}/Plugins/Samples/Housekeeper/Plugin.cpp
    ${HOUSEKEEPER_RESOURCES}
    )
  
  target_link_libraries(Housekeeper 
    ThirdPartyPlugins
    )
  
  set_target_properties(
    Housekeeper PROPERTIES 
    VERSION ${ORTHANC_VERSION} 
    SOVERSION ${ORTHANC_VERSION}
    )
  
  install(
    TARGETS Housekeeper
    RUNTIME DESTINATION lib    # Destination for Windows
    LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
    )
endif()


#####################################################################
## Build the companion tool to recover files compressed using Orthanc
#####################################################################

if (BUILD_RECOVER_COMPRESSED_FILE)
  set(RECOVER_COMPRESSED_SOURCES
    ${CMAKE_SOURCE_DIR}/Resources/Samples/Tools/RecoverCompressedFile.cpp
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
      COMMAND 
      ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/WindowsResources.py
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
    ${CMAKE_SOURCE_DIR}/Plugins/Include/orthanc/OrthancCPlugin.h 
    ${CMAKE_SOURCE_DIR}/Plugins/Include/orthanc/OrthancCDatabasePlugin.h 
    DESTINATION include/orthanc
    )
endif()



#####################################################################
## Prepare the "uninstall" target
## http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F
#####################################################################

configure_file(
    "${CMAKE_SOURCE_DIR}/../OrthancFramework/Resources/CMake/Uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
