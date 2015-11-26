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
##   - Orthanc >= 0.9.5: Boost 1.59.0

rm -rf /tmp/boost_1_59_0
rm -rf /tmp/bcp/boost_1_59_0

cd /tmp
echo "Uncompressing the sources of Boost 1.59.0..."
tar xfz ./boost_1_59_0.tar.gz 

echo "Generating the subset..."
mkdir -p /tmp/bcp/boost_1_59_0
bcp --boost=/tmp/boost_1_59_0 thread system locale date_time filesystem math/special_functions algorithm uuid atomic iostreams /tmp/bcp/boost_1_59_0
cd /tmp/bcp

echo "Compressing the subset..."
tar cfz boost_1_59_0_bcpdigest-0.9.5.tar.gz boost_1_59_0
ls -l boost_1_59_0_bcpdigest-0.9.5.tar.gz
md5sum boost_1_59_0_bcpdigest-0.9.5.tar.gz
readlink -f boost_1_59_0_bcpdigest-0.9.5.tar.gz
