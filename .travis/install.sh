#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
	# Install some dependencies
	brew update
	brew unlink cmake boost
	brew install cmake tbb assimp freeimage boost eigen

	# Install ctemplate
	# cd $HOME
	# git clone https://github.com/OlafvdSpek/ctemplate.git
	# cd ctemplate
	# ./configure
	# make -j
	# sudo make install

	# Install yaml-cpp
	cd $HOME
	git clone https://github.com/jbeder/yaml-cpp.git
	cd yaml-cpp
	mkdir build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release ..
	make -j
	sudo make install

	# Build lightmetrica
	cd $TRAVIS_BUILD_DIR
	mkdir build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release ..
	make -j

	# Packaging
	# cd $TRAVIS_BUILD_DIR
	# git clone --depth=1 https://github.com/hi2p-perim/lightmetrica-v2-example.git example
	# rm -rf example/.git example/.gitattributes
	# cpack -G "DragNDrop"
else
	docker build -t lightmetrica .
fi
