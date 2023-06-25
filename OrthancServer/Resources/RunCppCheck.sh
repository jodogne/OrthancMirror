#!/bin/bash

set -ex

CPPCHECK=cppcheck

if [ $# -ge 1 ]; then
    CPPCHECK=$1
fi

cat <<EOF > /tmp/cppcheck-suppressions.txt
constParameter:../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.cpp
knownArgument:../../OrthancFramework/UnitTestsSources/ImageTests.cpp
knownConditionTrueFalse:../../OrthancServer/Plugins/Engine/OrthancPlugins.cpp
nullPointer:../../OrthancFramework/UnitTestsSources/RestApiTests.cpp:315
stlFindInsert:../../OrthancFramework/Sources/DicomFormat/DicomMap.cpp:1476
stlFindInsert:../../OrthancFramework/Sources/RestApi/RestApiCallDocumentation.cpp:165
stlFindInsert:../../OrthancFramework/Sources/RestApi/RestApiCallDocumentation.cpp:73
stlFindInsert:../../OrthancServer/Sources/Database/StatelessDatabaseOperations.cpp:373
stlFindInsert:../../OrthancServer/Sources/OrthancWebDav.cpp:377
stlFindInsert:../../OrthancServer/Sources/ServerJobs/MergeStudyJob.cpp:40
stlFindInsert:../../OrthancServer/Sources/ServerJobs/SplitStudyJob.cpp:190
stlFindInsert:../../OrthancServer/Sources/ServerJobs/ResourceModificationJob.cpp:334
syntaxError:../../OrthancFramework/Sources/SQLite/FunctionContext.h:52
syntaxError:../../OrthancFramework/UnitTestsSources/DicomMapTests.cpp:73
syntaxError:../../OrthancFramework/UnitTestsSources/ZipTests.cpp:132
syntaxError:../../OrthancServer/UnitTestsSources/UnitTestsMain.cpp:310
uninitMemberVar:../../OrthancServer/Sources/ServerJobs/StorageCommitmentScpJob.cpp:416
unreadVariable:../../OrthancFramework/Sources/FileStorage/StorageAccessor.cpp
unreadVariable:../../OrthancServer/Sources/OrthancRestApi/OrthancRestModalities.cpp:1118
unusedFunction
useInitializationList:../../OrthancFramework/Sources/Images/PngReader.cpp:90
useInitializationList:../../OrthancFramework/Sources/Images/PngWriter.cpp:98
useInitializationList:../../OrthancServer/Sources/ServerJobs/DicomModalityStoreJob.cpp:274
assertWithSideEffect:../../OrthancServer/Plugins/Engine/OrthancPluginDatabase.cpp:276
assertWithSideEffect:../../OrthancServer/Plugins/Engine/OrthancPluginDatabase.cpp:1025
assertWithSideEffect:../../OrthancServer/Sources/Database/Compatibility/DatabaseLookup.cpp:289
assertWithSideEffect:../../OrthancServer/Sources/Database/Compatibility/DatabaseLookup.cpp:388
assertWithSideEffect:../../OrthancServer/Sources/Database/StatelessDatabaseOperations.cpp:3551
assertWithSideEffect:../../OrthancServer/Sources/ServerJobs/ResourceModificationJob.cpp:272
assertWithSideEffect:../../OrthancFramework/Sources/DicomNetworking/Internals/CommandDispatcher.cpp:453
EOF

${CPPCHECK} --enable=all --quiet --std=c++11 \
            --suppressions-list=/tmp/cppcheck-suppressions.txt \
            -DBOOST_HAS_DATE_TIME=1 \
            -DBOOST_HAS_FILESYSTEM_V3=1 \
            -DBOOST_HAS_REGEX=1 \
            -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=1 \
            -DCIVETWEB_HAS_WEBDAV_WRITING=1 \
            -DDCMTK_VERSION_NUMBER=365 \
            -DHAVE_MALLOPT=1 \
            -DHAVE_MALLOC_TRIM=1 \
            -DMONGOOSE_USE_CALLBACKS=1 \
            -DJSONCPP_VERSION_MAJOR=1 \
            -DJSONCPP_VERSION_MINOR=0 \
            -DORTHANC_BUILDING_FRAMEWORK_LIBRARY=0 \
            -DORTHANC_BUILDING_SERVER_LIBRARY=1 \
            -DORTHANC_BUILD_UNIT_TESTS=1 \
            -DORTHANC_ENABLE_BASE64=1 \
            -DORTHANC_ENABLE_CIVETWEB=1 \
            -DORTHANC_ENABLE_CURL=1 \
            -DORTHANC_ENABLE_DCMTK=1 \
            -DORTHANC_ENABLE_DCMTK_JPEG=1 \
            -DORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS=1 \
            -DORTHANC_ENABLE_DCMTK_NETWORKING=1 \
            -DORTHANC_ENABLE_DCMTK_TRANSCODING=1 \
            -DORTHANC_ENABLE_JPEG=1 \
            -DORTHANC_ENABLE_LOCALE=1 \
            -DORTHANC_ENABLE_LOGGING=1 \
            -DORTHANC_ENABLE_LOGGING_STDIO=1 \
            -DORTHANC_ENABLE_LUA=1 \
            -DORTHANC_ENABLE_MD5=1 \
            -DORTHANC_ENABLE_MONGOOSE=0 \
            -DORTHANC_ENABLE_PKCS11=1 \
            -DORTHANC_ENABLE_PLUGINS=1 \
            -DORTHANC_ENABLE_PNG=1 \
            -DORTHANC_ENABLE_PUGIXML=1 \
            -DORTHANC_ENABLE_SQLITE=1 \
            -DORTHANC_ENABLE_SSL=1 \
            -DORTHANC_ENABLE_ZLIB=1 \
            -DORTHANC_SANDBOXED=0 \
            -DORTHANC_SQLITE_VERSION=3027001 \
            -DORTHANC_UNIT_TESTS_LINK_FRAMEWORK=1 \
            -DPUGIXML_VERSION=150 \
            -DUNIT_TESTS_WITH_HTTP_CONNEXIONS=1 \
            -D__BYTE_ORDER=__LITTLE_ENDIAN \
            -D__GNUC__ \
            -D__cplusplus=201103 \
            -D__linux__ \
            -UNDEBUG \
            -DHAS_ORTHANC_EXCEPTION=1 \
            \
            ../../OrthancFramework/Sources \
            ../../OrthancFramework/UnitTestsSources \
            ../../OrthancServer/Plugins/Engine \
            ../../OrthancServer/Plugins/Include \
            ../../OrthancServer/Sources \
            ../../OrthancServer/UnitTestsSources \
            ../../OrthancServer/Plugins/Samples/Common \
            ../../OrthancServer/Plugins/Samples/ConnectivityChecks \
            ../../OrthancServer/Plugins/Samples/DelayedDeletion \
            ../../OrthancServer/Plugins/Samples/Housekeeper \
            ../../OrthancServer/Plugins/Samples/ModalityWorklists \
            ../../OrthancServer/Plugins/Samples/MultitenantDicom \
            \
            2>&1
