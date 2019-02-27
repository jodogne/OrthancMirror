#!/bin/bash

set -e
set -u

## Starting with version 0.6.2, Orthanc is shipped with a subset of the
## Boost libraries that is generated with the BCP tool:
##
## http://www.boost.org/doc/libs/1_54_0/tools/bcp/doc/html/index.html
##
## This script generates this subset.
##
## History:
##   - Orthanc between 0.6.2 and 0.7.3: Boost 1.54.0
##   - Orthanc between 0.7.4 and 0.9.1: Boost 1.55.0
##   - Orthanc between 0.9.2 and 0.9.4: Boost 1.58.0
##   - Orthanc between 0.9.5 and 1.0.0: Boost 1.59.0
##   - Orthanc between 1.1.0 and 1.2.0: Boost 1.60.0
##   - Orthanc 1.3.0: Boost 1.64.0
##   - Orthanc 1.3.1: Boost 1.65.1
##   - Orthanc 1.3.2: Boost 1.66.0
##   - Orthanc between 1.4.0 and 1.4.2: Boost 1.67.0
##   - Orthanc between 1.5.0 and 1.5.4: Boost 1.68.0
##   - Orthanc >= 1.5.5: Boost 1.69.0

BOOST_VERSION=1_69_0
ORTHANC_VERSION=1.5.6

rm -rf /tmp/boost_${BOOST_VERSION}
rm -rf /tmp/bcp/boost_${BOOST_VERSION}

cd /tmp
echo "Uncompressing the sources of Boost ${BOOST_VERSION}..."
tar xfz ./boost_${BOOST_VERSION}.tar.gz 

echo "Generating the subset..."
mkdir -p /tmp/bcp/boost_${BOOST_VERSION}
bcp --boost=/tmp/boost_${BOOST_VERSION} thread system locale date_time filesystem math/special_functions algorithm uuid atomic iostreams program_options numeric/ublas geometry polygon signals2 chrono /tmp/bcp/boost_${BOOST_VERSION}

echo "Removing documentation..."
rm -rf /tmp/bcp/boost_${BOOST_VERSION}/libs/locale/doc/html
rm -rf /tmp/bcp/boost_${BOOST_VERSION}/libs/algorithm/doc/html
rm -rf /tmp/bcp/boost_${BOOST_VERSION}/libs/geometry/doc/html
rm -rf /tmp/bcp/boost_${BOOST_VERSION}/libs/geometry/doc/doxy/doxygen_output/html
rm -rf /tmp/bcp/boost_${BOOST_VERSION}/libs/filesystem/example/

# https://stackoverflow.com/questions/1655372/longest-line-in-a-file
LONGEST_FILENAME=`find /tmp/bcp/ | awk '{print length, $0}' | sort -nr | head -1`
LONGEST=`echo "$LONGEST_FILENAME" | cut -d ' ' -f 1`

echo
echo "Longest filename (${LONGEST} characters):"
echo "${LONGEST_FILENAME}"
echo

if [ ${LONGEST} -ge 128 ]; then
    echo "ERROR: Too long filename for Windows!"
    echo
    exit -1
fi

echo "Compressing the subset..."
cd /tmp/bcp
tar cfz boost_${BOOST_VERSION}_bcpdigest-${ORTHANC_VERSION}.tar.gz boost_${BOOST_VERSION}
ls -l boost_${BOOST_VERSION}_bcpdigest-${ORTHANC_VERSION}.tar.gz
md5sum boost_${BOOST_VERSION}_bcpdigest-${ORTHANC_VERSION}.tar.gz
readlink -f boost_${BOOST_VERSION}_bcpdigest-${ORTHANC_VERSION}.tar.gz
