FROM ubuntu:14.04
MAINTAINER Hisanari Otsu <hi2p.perim@gmail.com>

# Install some packages
RUN apt-get update -qq
RUN apt-get install -qq -y git software-properties-common 
RUN add-apt-repository -y ppa:george-edison55/cmake-3.x
RUN apt-get update -qq
RUN apt-get install -qq -y python cmake build-essential freeglut3-dev libxmu-dev libxi-dev libtbb-dev libeigen3-dev wget unzip

# Update to gcc-4.9
RUN sudo add-apt-repository ppa:ubuntu-toolchain-r/test
RUN sudo apt-get update -qq
RUN sudo apt-get install -y gcc-4.9 g++-4.9
RUN sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.9

# Install boost
RUN wget -O boost_1_60_0.tar.bz2 "http://sourceforge.net/projects/boost/files/boost/1.60.0/boost_1_60_0.tar.bz2/download"
RUN tar --bzip2 -xf boost_1_60_0.tar.bz2
RUN cd boost_1_60_0 && ./bootstrap.sh --with-libraries=program_options,filesystem,system,regex,coroutine,context && ./b2 cxxflags=-fPIC cflags=-fPIC link=static -j8

# Install freeimage
RUN wget http://downloads.sourceforge.net/freeimage/FreeImage3170.zip
RUN unzip FreeImage3170.zip
RUN cd FreeImage && make -j 1 && make install

# Install assimp
RUN git clone --depth=1 --branch v3.1.1 https://github.com/assimp/assimp.git assimp
RUN mkdir -p assimp/build && cd assimp/build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j 1 && make install

# Install embree
RUN git clone --depth=1 --branch v2.8.0 https://github.com/embree/embree.git embree
RUN mkdir -p embree/build && cd embree/build && cmake -D CMAKE_BUILD_TYPE=Release -D ENABLE_ISPC_SUPPORT=OFF -D RTCORE_TASKING_SYSTEM=INTERNAL -D ENABLE_TUTORIALS=OFF .. && make -j 1 && make install && cp libembree.so /usr/local/lib

# Install google-ctemplate
RUN git clone --depth=1 https://github.com/OlafvdSpek/ctemplate.git ctemplate
RUN cd ctemplate && ./configure --enable-shared --with-pic && make -j 1 && make install

# Install yaml-cpp
RUN git clone --depth=1 https://github.com/jbeder/yaml-cpp.git yaml-cpp
RUN mkdir -p yaml-cpp/build && cd yaml-cpp/build && cmake -DCMAKE_BUILD_TYPE=Release -D BUILD_SHARED_LIBS=ON .. && make -j 1 && make install

# Add a project file to the container
COPY . /lightmetrica/

# Avois clock skew detected warning
RUN find /lightmetrica -print0 | xargs -0 touch

# Build lightmetrica
RUN mkdir -p lightmetrica/build && cd lightmetrica/build && BOOST_ROOT="" BOOST_INCLUDEDIR="/boost_1_60_0" BOOST_LIBRARYDIR="/boost_1_60_0/stage/lib" cmake -DCMAKE_BUILD_TYPE=Release .. && make -j 1
ENV PATH /lightmetrica/build/bin/Release:$PATH
ENTRYPOINT ["lightmetrica"]
