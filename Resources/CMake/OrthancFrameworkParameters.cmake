#####################################################################
## Versioning information
#####################################################################

# Version of the build, should always be "mainline" except in release branches
set(ORTHANC_VERSION "mainline")

# Version of the database schema. History:
#   * Orthanc 0.1.0 -> Orthanc 0.3.0 = no versioning
#   * Orthanc 0.3.1                  = version 2
#   * Orthanc 0.4.0 -> Orthanc 0.7.2 = version 3
#   * Orthanc 0.7.3 -> Orthanc 0.8.4 = version 4
#   * Orthanc 0.8.5 -> Orthanc 0.9.4 = version 5
#   * Orthanc 0.9.5 -> mainline      = version 6
set(ORTHANC_DATABASE_VERSION 6)

# Version of the Orthanc API, can be retrieved from "/system" URI in
# order to check whether new URI endpoints are available even if using
# the mainline version of Orthanc
set(ORTHANC_API_VERSION "1.0")


#####################################################################
## CMake parameters tunable by the user
#####################################################################

# Support of static compilation
set(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")
set(STATIC_BUILD OFF CACHE BOOL "Static build of the third-party libraries (necessary for Windows)")
set(STANDALONE_BUILD ON CACHE BOOL "Standalone build (all the resources are embedded, necessary for releases)")

# Generic parameters of the build
set(ENABLE_CIVETWEB OFF CACHE BOOL "Use Civetweb instead of Mongoose (experimental)")
set(ENABLE_PKCS11 OFF CACHE BOOL "Enable PKCS#11 for HTTPS client authentication using hardware security modules and smart cards")
set(ENABLE_PROFILING OFF CACHE BOOL "Whether to enable the generation of profiling information with gprof")
set(ENABLE_SSL ON CACHE BOOL "Include support for SSL")
set(ENABLE_LUA_MODULES OFF CACHE BOOL "Enable support for loading external Lua modules (only meaningful if using static version of the Lua engine)")

# Parameters to fine-tune linking against system libraries
set(USE_SYSTEM_BOOST ON CACHE BOOL "Use the system version of Boost")
set(USE_SYSTEM_CIVETWEB ON CACHE BOOL "Use the system version of Civetweb (experimental)")
set(USE_SYSTEM_CURL ON CACHE BOOL "Use the system version of LibCurl")
set(USE_SYSTEM_GOOGLE_TEST ON CACHE BOOL "Use the system version of Google Test")
set(USE_SYSTEM_JSONCPP ON CACHE BOOL "Use the system version of JsonCpp")
set(USE_SYSTEM_LIBICONV ON CACHE BOOL "Use the system version of libiconv")
set(USE_SYSTEM_LIBJPEG ON CACHE BOOL "Use the system version of libjpeg")
set(USE_SYSTEM_LIBP11 OFF CACHE BOOL "Use the system version of libp11 (PKCS#11 wrapper library)")
set(USE_SYSTEM_LIBPNG ON CACHE BOOL "Use the system version of libpng")
set(USE_SYSTEM_LUA ON CACHE BOOL "Use the system version of Lua")
set(USE_SYSTEM_MONGOOSE ON CACHE BOOL "Use the system version of Mongoose")
set(USE_SYSTEM_OPENSSL ON CACHE BOOL "Use the system version of OpenSSL")
set(USE_SYSTEM_PUGIXML ON CACHE BOOL "Use the system version of Pugixml")
set(USE_SYSTEM_SQLITE ON CACHE BOOL "Use the system version of SQLite")
set(USE_SYSTEM_UUID ON CACHE BOOL "Use the system version of the uuid library from e2fsprogs")
set(USE_SYSTEM_ZLIB ON CACHE BOOL "Use the system version of ZLib")

# Parameters specific to DCMTK
set(DCMTK_DICTIONARY_DIR "" CACHE PATH "Directory containing the DCMTK dictionaries \"dicom.dic\" and \"private.dic\" (only when using system version of DCMTK)") 
set(USE_DCMTK_360 OFF CACHE BOOL "Use older DCMTK version 3.6.0 in static builds (instead of default 3.6.2)")
set(USE_DCMTK_362_PRIVATE_DIC ON CACHE BOOL "Use the dictionary of private tags from DCMTK 3.6.2 if using DCMTK 3.6.0")
set(USE_SYSTEM_DCMTK ON CACHE BOOL "Use the system version of DCMTK")
set(ENABLE_DCMTK_JPEG ON CACHE BOOL "Enable JPEG-LS (Lossless) decompression")
set(ENABLE_DCMTK_JPEG_LOSSLESS ON CACHE BOOL "Enable JPEG-LS (Lossless) decompression")

# Advanced and distribution-specific parameters
set(USE_GOOGLE_TEST_DEBIAN_PACKAGE OFF CACHE BOOL "Use the sources of Google Test shipped with libgtest-dev (Debian only)")
set(SYSTEM_MONGOOSE_USE_CALLBACKS ON CACHE BOOL "The system version of Mongoose uses callbacks (version >= 3.7)")
set(USE_BOOST_ICONV ON CACHE BOOL "Use iconv instead of wconv (Windows only)")
set(USE_PUGIXML ON CACHE BOOL "Use the Pugixml parser (turn off only for debug)")

mark_as_advanced(USE_GOOGLE_TEST_DEBIAN_PACKAGE)
mark_as_advanced(SYSTEM_MONGOOSE_USE_CALLBACKS)
mark_as_advanced(USE_BOOST_ICONV)
mark_as_advanced(USE_PUGIXML)


#####################################################################
## Internal CMake parameters to enable the optional subcomponents of
## the Orthanc framework
#####################################################################

# These options must be set to "ON" if compiling Orthanc, but might be
# set to "OFF" by third-party projects if their associated features
# are not required

set(ENABLE_CRYPTO_OPTIONS OFF CACHE INTERNAL "Show options related to cryptography")
set(ENABLE_JPEG OFF CACHE INTERNAL "Enable support of JPEG")
set(ENABLE_GOOGLE_TEST OFF CACHE INTERNAL "Enable support of Google Test")
set(ENABLE_LOCALE OFF CACHE INTERNAL "Enable support for locales (notably in Boost)")
set(ENABLE_LUA OFF CACHE INTERNAL "Enable support of Lua scripting")
set(ENABLE_PNG OFF CACHE INTERNAL "Enable support of PNG")
set(ENABLE_PUGIXML OFF CACHE INTERNAL "Enable support of XML through Pugixml")
set(ENABLE_SQLITE OFF CACHE INTERNAL "Enable support of SQLite databases")
set(ENABLE_WEB_CLIENT OFF CACHE INTERNAL "Enable Web client")
set(ENABLE_WEB_SERVER OFF CACHE INTERNAL "Enable embedded Web server")
set(ENABLE_DCMTK OFF CACHE INTERNAL "Enable DCMTK")
set(ENABLE_DCMTK_NETWORKING OFF CACHE INTERNAL "Enable DICOM networking in DCMTK")

set(HAS_EMBEDDED_RESOURCES OFF CACHE INTERNAL
  "Whether resources are auto-generated using EmbedResources.py")

set(ORTHANC_SANDBOXED OFF CACHE INTERNAL
  "Whether Orthanc runs inside a sandboxed environment (such as Google NaCl or WebAssembly)")
