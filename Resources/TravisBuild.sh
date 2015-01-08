#!/bin/bash

set -e

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    cd BuildMinGW32
    make
    cd ..
fi

cd Build
make
./UnitTests
