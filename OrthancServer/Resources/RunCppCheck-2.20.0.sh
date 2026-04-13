#!/bin/bash
# Note: This script is tuned to run with cppcheck v2.17.1

set -ex

CPPCHECK=cppcheck

if [ $# -ge 1 ]; then
    CPPCHECK=$1
fi

cat <<EOF > /tmp/cppcheck-suppressions.txt
EOF

CPPCHECK_BUILD_DIR=/tmp/cppcheck-build-dir-2.20.0/
mkdir -p ${CPPCHECK_BUILD_DIR}

${CPPCHECK} -j8 --enable=all --quiet --std=c++11 \
            --cppcheck-build-dir=${CPPCHECK_BUILD_DIR} \
            --platform=unix64 \
            --language=c++ --suppress=missingIncludeSystem --library=boost \
            --suppress=unusedFunction --suppress=normalCheckLevelMaxBranches --suppress=useStlAlgorithm \
            --suppressions-list=/tmp/cppcheck-suppressions.txt \
            -D__BYTE_ORDER=__LITTLE_ENDIAN \
            -D__GNUC__ \
            -D__linux__ \
            \
            '-DORTHANC_FRAMEWORK_VERSION_IS_ABOVE(a,b,c)=true' \
            -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=1 \
            -DCIVETWEB_HAS_WEBDAV_WRITING=1 \
            -DDCMTK_VERSION_NUMBER=370 \
            -DHAS_ORTHANC_EXCEPTION=1 \
            -DHAVE_MALLOPT=1 \
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
            -DORTHANC_UNIT_TESTS_LINK_FRAMEWORK=1 \
            -DORTHANC_USE_SYSTEM_MINIZIP=1 \
            -DORTHANC_VERSION="\"mainline\"" \
            -DPUGIXML_VERSION=150 \
            -DUNIT_TESTS_WITH_HTTP_CONNEXIONS=1 \
            \
            ../../OrthancFramework/Sources \
            ../../OrthancFramework/UnitTestsSources \
            ../../OrthancServer/Plugins/Engine \
            ../../OrthancServer/Plugins/Include \
            ../../OrthancServer/Sources \
            ../../OrthancServer/UnitTestsSources \
            ../../OrthancServer/Plugins/Samples/Common \
            \
            ../../OrthancServer/Plugins/Samples/AdoptDicomInstance/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/AutomatedJpeg2kCompression/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/Common/OrthancPluginCppWrapper.cpp \
            ../../OrthancServer/Plugins/Samples/ConnectivityChecks/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/CppSkeleton/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/CustomImageDecoder/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/DelayedDeletion/LargeDeleteJob.cpp \
            ../../OrthancServer/Plugins/Samples/DelayedDeletion/PendingDeletionsDatabase.cpp \
            ../../OrthancServer/Plugins/Samples/DelayedDeletion/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/Housekeeper/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/ModalityWorklists/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/MultitenantDicom/DicomFilter.cpp \
            ../../OrthancServer/Plugins/Samples/MultitenantDicom/FindRequestHandler.cpp \
            ../../OrthancServer/Plugins/Samples/MultitenantDicom/MoveRequestHandler.cpp \
            ../../OrthancServer/Plugins/Samples/MultitenantDicom/MultitenantDicomServer.cpp \
            ../../OrthancServer/Plugins/Samples/MultitenantDicom/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/MultitenantDicom/PluginToolbox.cpp \
            ../../OrthancServer/Plugins/Samples/MultitenantDicom/StoreRequestHandler.cpp \
            ../../OrthancServer/Plugins/Samples/Sanitizer/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/StorageArea/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/StorageCommitmentScp/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/WebDavFilesystem/Plugin.cpp \
            ../../OrthancServer/Plugins/Samples/WebSkeleton/Framework/Plugin.cpp \
            \
            2>&1
