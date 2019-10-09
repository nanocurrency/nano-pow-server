#!/bin/bash

set -o errexit
set -o nounset
set -o xtrace
OS=`uname`

if [[ "$OS" == 'Linux' ]]; then
    aws s3 cp $TRAVIS_BUILD_DIR/build/nano_pow_server-*-Linux.tar.gz s3://$AWS_BUCKET/pow-server/$TRAVIS_BRANCH/nano_pow_server-$TRAVIS_COMMIT-Linux.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
    aws s3 cp $TRAVIS_BUILD_DIR/build/nano_pow_server-*-Linux.tar.gz s3://$AWS_BUCKET/pow-server/$TRAVIS_BRANCH/nano_pow_server-latest-Linux.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
elif [[ "$OS" == 'Darwin' ]]; then
    aws s3 cp $TRAVIS_BUILD_DIR/build/nano_pow_server-*-Darwin.tar.gz s3://$AWS_BUCKET/pow-server/$TRAVIS_BRANCH/nano_pow_server-$TRAVIS_COMMIT-Darwin.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
    aws s3 cp $TRAVIS_BUILD_DIR/build/nano_pow_server-*-Darwin.tar.gz s3://$AWS_BUCKET/pow-server/$TRAVIS_BRANCH/nano_pow_server-latest-Darwin.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
else
	/c/Program\ Files/Amazon/AWSCLI/bin/aws s3 cp $TRAVIS_BUILD_DIR/build/nano_pow_server-*-win64.tar.gz s3://$AWS_BUCKET/pow-server/$TRAVIS_BRANCH/nano_pow_server-$TRAVIS_COMMIT-win64.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
	/c/Program\ Files/Amazon/AWSCLI/bin/aws s3 cp $TRAVIS_BUILD_DIR/build/nano_pow_server-*-win64.tar.gz s3://$AWS_BUCKET/pow-server/$TRAVIS_BRANCH/nano_pow_server-latest-win64.tar.gz --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
fi
