#!/bin/bash
# Note: This script is tuned to run with cppcheck v2.17.0

set -ex

CPPCHECK=cppcheck

if [ $# -ge 1 ]; then
    CPPCHECK=$1
fi

cat <<EOF > /tmp/cppcheck-suppressions.txt
nullPointer:../../OrthancFramework/UnitTestsSources/RestApiTests.cpp:322
stlFindInsert:../../OrthancFramework/Sources/DicomFormat/DicomMap.cpp:1525
stlFindInsert:../../OrthancFramework/Sources/RestApi/RestApiCallDocumentation.cpp:166
stlFindInsert:../../OrthancFramework/Sources/RestApi/RestApiCallDocumentation.cpp:74
stlFindInsert:../../OrthancServer/Sources/Database/MainDicomTagsRegistry.cpp:65
stlFindInsert:../../OrthancServer/Sources/OrthancWebDav.cpp:328
stlFindInsert:../../OrthancServer/Sources/ServerJobs/MergeStudyJob.cpp:41
stlFindInsert:../../OrthancServer/Sources/ServerJobs/SplitStudyJob.cpp:191
stlFindInsert:../../OrthancServer/Sources/ServerJobs/ResourceModificationJob.cpp:361
syntaxError:../../OrthancFramework/Sources/SQLite/FunctionContext.h:53
syntaxError:../../OrthancFramework/UnitTestsSources/DicomMapTests.cpp:74
syntaxError:../../OrthancFramework/UnitTestsSources/ZipTests.cpp:133
syntaxError:../../OrthancServer/UnitTestsSources/UnitTestsMain.cpp:322
uninitMemberVar:../../OrthancServer/Sources/ServerJobs/StorageCommitmentScpJob.cpp:417
unreadVariable:../../OrthancServer/Sources/OrthancRestApi/OrthancRestModalities.cpp:1173
useInitializationList:../../OrthancFramework/Sources/Images/PngReader.cpp:91
useInitializationList:../../OrthancFramework/Sources/Images/PngWriter.cpp:99
useInitializationList:../../OrthancServer/Sources/ServerJobs/DicomModalityStoreJob.cpp:275
assertWithSideEffect:../../OrthancServer/Plugins/Engine/OrthancPluginDatabase.cpp:277
assertWithSideEffect:../../OrthancServer/Plugins/Engine/OrthancPluginDatabase.cpp:1026
assertWithSideEffect:../../OrthancServer/Sources/Database/Compatibility/DatabaseLookup.cpp:292
assertWithSideEffect:../../OrthancServer/Sources/Database/Compatibility/DatabaseLookup.cpp:391
assertWithSideEffect:../../OrthancServer/Sources/Database/StatelessDatabaseOperations.cpp:3058
assertWithSideEffect:../../OrthancServer/Sources/ServerJobs/ResourceModificationJob.cpp:286
assertWithSideEffect:../../OrthancFramework/Sources/DicomNetworking/Internals/CommandDispatcher.cpp:454
EOF

${CPPCHECK} --enable=all --std=gnu++11 --library=boost \
            --suppressions-list=/tmp/cppcheck-suppressions.txt \
            -I/usr/include/ \
            -I/usr/include/jsoncpp/ \
            -I/usr/include/linux/ \
            -I/usr/include/c++/11/ \
            -I/usr/include/c++/11/tr1/ \
            -I/usr/include/x86_64-linux-gnu/c++/11/ \
            -DBOOST_HAS_DATE_TIME=1 \
            -DBOOST_HAS_FILESYSTEM_V3=1 \
            -DBOOST_HAS_REGEX=1 \
            -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=1 \
            -DCIVETWEB_HAS_WEBDAV_WRITING=1 \
            -DDCMTK_VERSION_NUMBER=369 \
            -DJCONFIG_INCLUDED \
            -DHAVE_MALLOPT=1 \
            -DMONGOOSE_USE_CALLBACKS=1 \
            -DJSONCPP_VERSION_MAJOR=1 \
            -DJSONCPP_VERSION_MINOR=0 \
            -DORTHANC_BUILDING_FRAMEWORK_LIBRARY=0 \
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
            -DORTHANC_UNIT_TESTS_LINK_FRAMEWORK=0 \
            -DPUGIXML_VERSION=150 \
            -DUNIT_TESTS_WITH_HTTP_CONNEXIONS=1 \
            -D__BYTE_ORDER=__LITTLE_ENDIAN \
            -D__GNUC__=11 \
            -D__GNUC_MINOR__=4 \
            -D__GNUC_PATCHLEVEL_=0 \
            -D__STDC__ \
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
            ../../OrthancServer/Plugins/Samples/AdoptDicomInstance \
            \
            2>&1
