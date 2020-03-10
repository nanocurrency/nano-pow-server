#!/bin/bash

set -o errexit
set -o nounset
set -o xtrace
OS=`uname`

if [[ ${ARTIFACT-0} -eq 1 ]]; then
    RUN="artifact"
    TESTS="OFF"
else
    RUN="test"
    TESTS="ON"
fi

mkdir build
pushd build

echo "build ${TESTS}"
cmake ..\
    -DNANO_POW_STANDALONE=ON \
    -DNANO_POW_SERVER_TEST=${TESTS}

if [[ ${RUN} == "artifact" ]]; then
    if [[ "$OS" == 'Linux' ]]; then
        cmake --build . \
            --target package \
            --config Release \
            -- -j2
    else
        sudo cmake --build . \
            --target package \
            --config Release \
            -- -j2
    fi
else
    if [[ "$OS" == 'Linux' ]]; then
        cmake --build . \
            --target tests \
            --config Debug \
            -- -j2
    else
        sudo cmake --build . \
            --target tests \
            --config Debug \
            -- -j2
    fi
    ./tests
fi