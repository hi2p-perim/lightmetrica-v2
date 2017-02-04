#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
	cd $TRAVIS_BUILD_DIR/dist/bin/Release
	./lightmetrica
else
	docker run lightmetrica
fi
