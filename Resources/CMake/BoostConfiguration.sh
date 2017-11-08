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
##   - Orthanc >= 1.3.1: Boost 1.65.1

BOOST_VERSION=1_65_1
ORTHANC_VERSION=1.3.1

rm -rf /tmp/boost_${BOOST_VERSION}
rm -rf /tmp/bcp/boost_${BOOST_VERSION}

cd /tmp
echo "Uncompressing the sources of Boost ${BOOST_VERSION}..."
tar xfz ./boost_${BOOST_VERSION}.tar.gz 

echo "Generating the subset..."
mkdir -p /tmp/bcp/boost_${BOOST_VERSION}
bcp --boost=/tmp/boost_${BOOST_VERSION} thread system locale date_time filesystem math/special_functions algorithm uuid atomic iostreams program_options numeric/ublas geometry polygon /tmp/bcp/boost_${BOOST_VERSION}
cd /tmp/bcp

echo "Compressing the subset..."
tar cfz boost_${BOOST_VERSION}_bcpdigest-${ORTHANC_VERSION}.tar.gz boost_${BOOST_VERSION}
ls -l boost_${BOOST_VERSION}_bcpdigest-${ORTHANC_VERSION}.tar.gz
md5sum boost_${BOOST_VERSION}_bcpdigest-${ORTHANC_VERSION}.tar.gz
readlink -f boost_${BOOST_VERSION}_bcpdigest-${ORTHANC_VERSION}.tar.gz
