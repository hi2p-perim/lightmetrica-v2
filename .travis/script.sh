#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
	cd $TRAVIS_BUILD_DIR/build/bin/Release
	./lightmetrica
else
	docker run lightmetrica
fi
