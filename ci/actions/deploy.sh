#!/bin/bash

set -o errexit
set -o nounset
set -o xtrace
OS=`uname`

if [[ "$OS" == 'Linux' ]]; then
    aws s3 cp $GITHUB_WORKSPACE/build/nano_pow_server-*-Linux.tar.gz s3://repo.nano.org/pow-server/nano_pow_server-$TAG-Linux.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
    aws s3 cp $GITHUB_WORKSPACE/build/nano_pow_server-*-Linux.tar.gz s3://repo.nano.org/pow-server/nano_pow_server-latest-Linux.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
else
    aws s3 cp $GITHUB_WORKSPACE/build/nano_pow_server-*-Darwin.tar.gz s3://repo.nano.org/pow-server/nano_pow_server-$TAG-Darwin.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
    aws s3 cp $GITHUB_WORKSPACE/build/nano_pow_server-*-Darwin.tar.gz s3://repo.nano.org/pow-server/nano_pow_server-latest-Darwin.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
fi