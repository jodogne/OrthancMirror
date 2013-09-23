#!/bin/bash

set -e
set -u

## Starting with version 0.6.2, Orthanc is shipped with a subset of the
## Boost libraries that is generated with the BCP tool:
##
## http://www.boost.org/doc/libs/1_54_0/tools/bcp/doc/html/index.html
##
## This script generates this subset.

rm -rf /tmp/boost_1_54_0
rm -rf /tmp/bcp/boost_1_54_0

cd /tmp
echo "Uncompressing the source of Boost 1.54.0..."
tar xfz boost_1_54_0.tar.gz 

echo "Generating the subset..."
mkdir -p /tmp/bcp/boost_1_54_0
bcp --boost=/tmp/boost_1_54_0 thread system locale date_time filesystem math/special_functions algorithm /tmp/bcp/boost_1_54_0
cd /tmp/bcp

echo "Compressing the subset..."
tar cfz boost_1_54_0_bcpdigest.tar.gz boost_1_54_0
ls -l boost_1_54_0_bcpdigest.tar.gz
md5sum boost_1_54_0_bcpdigest.tar.gz
