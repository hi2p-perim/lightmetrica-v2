FROM ubuntu:14.04
MAINTAINER Hisanari Otsu <hi2p.perim@gmail.com>

# --------------------------------------------------------------------------------

# Install some packages
RUN apt-get update -qq && apt-get install -qq -y \
    git \
    software-properties-common \
    python \
    python-jinja2 \
    build-essential \
    freeglut3-dev \
    libxmu-dev \
    libxi-dev \
    libtbb-dev \
    libeigen3-dev \
    wget \
    unzip \
    curl

# --------------------------------------------------------------------------------

# cmake-3
RUN add-apt-repository -y ppa:george-edison55/cmake-3.x
RUN apt-get update -qq && apt-get install -qq -y cmake

# --------------------------------------------------------------------------------

# Update to gcc-4.9
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt-get update -qq && apt-get install -y gcc-4.9 g++-4.9
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.9

# --------------------------------------------------------------------------------

# Boost
WORKDIR /
RUN curl -sL "http://sourceforge.net/projects/boost/files/boost/1.60.0/boost_1_60_0.tar.bz2/download" | tar xj
WORKDIR /boost_1_60_0 
RUN ./bootstrap.sh --with-libraries=program_options,filesystem,system,regex,coroutine,context
RUN ./b2 cxxflags=-fPIC cflags=-fPIC link=static -j8

# --------------------------------------------------------------------------------


# FreeImage
WORKDIR /
RUN wget http://downloads.sourceforge.net/freeimage/FreeImage3170.zip
RUN unzip FreeImage3170.zip
WORKDIR /FreeImage
RUN make -j 1 && make install

# --------------------------------------------------------------------------------

# Assimp
WORKDIR /
RUN git clone --depth=1 --branch v3.1.1 https://github.com/assimp/assimp.git assimp
WORKDIR /assimp/build
RUN cmake -DCMAKE_BUILD_TYPE=Release .. && make -j 1 && make install

# --------------------------------------------------------------------------------

# Embree
WORKDIR /
RUN git clone --depth=1 --branch v2.8.0 https://github.com/embree/embree.git embree
WORKDIR /embree/build
RUN cmake -D CMAKE_BUILD_TYPE=Release -D ENABLE_ISPC_SUPPORT=OFF -D RTCORE_TASKING_SYSTEM=INTERNAL -D ENABLE_TUTORIALS=OFF .. && make -j 1 && make install && cp libembree.so /usr/local/lib

# --------------------------------------------------------------------------------

# yaml-cpp
WORKDIR /
RUN git clone --depth=1 https://github.com/jbeder/yaml-cpp.git yaml-cpp
WORKDIR /yaml-cpp/build
RUN cmake -DCMAKE_BUILD_TYPE=Release -D BUILD_SHARED_LIBS=ON .. && make -j 1 && make install

# --------------------------------------------------------------------------------

# Add a project file to the container
WORKDIR /
COPY . /lightmetrica/

# Avois clock skew detected warning
RUN find /lightmetrica -print0 | xargs -0 touch

# Build lightmetrica
WORKDIR /lightmetrica/build
RUN BOOST_ROOT="" BOOST_INCLUDEDIR="/boost_1_60_0" BOOST_LIBRARYDIR="/boost_1_60_0/stage/lib" cmake -DCMAKE_BUILD_TYPE=Release -D LM_USE_SINGLE_PRECISION=OFF -D LM_USE_DOUBLE_PRECISION=ON .. && make -j
ENV PATH /lightmetrica/dist/bin/Release:$PATH
#ENTRYPOINT ["lightmetrica"]

