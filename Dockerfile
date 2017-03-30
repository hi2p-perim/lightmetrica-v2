FROM jupyter/scipy-notebook
MAINTAINER Hisanari Otsu <hi2p.perim@gmail.com>

ENV BUILD_CORES -j
ENV BUILD_CORES_BOOST -j8

USER root

# --------------------------------------------------------------------------------

# Install some packages
RUN apt-get update -qq && apt-get install -qq -y \
    git \
    software-properties-common \
    build-essential \
    freeglut3-dev \
    libxmu-dev \
    libxi-dev \
    libeigen3-dev \
    wget \
    unzip \
    curl

# --------------------------------------------------------------------------------

# cmake-3
#RUN add-apt-repository -y ppa:george-edison55/cmake-3.x
#RUN apt-get update -qq && apt-get install -qq -y cmake

WORKDIR /
RUN wget http://www.cmake.org/files/v3.7/cmake-3.7.1.tar.gz
RUN tar xf cmake-3.7.1.tar.gz
WORKDIR /cmake-3.7.1
RUN ./bootstrap
RUN make $BUILD_CORES && make install

# --------------------------------------------------------------------------------

# Update to gcc-4.9
#RUN add-apt-repository ppa:ubuntu-toolchain-r/test
#RUN apt-get update -qq && apt-get install -y gcc-4.9 g++-4.9
#RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.9

# --------------------------------------------------------------------------------

# Boost
WORKDIR /
RUN curl -sL "http://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.tar.bz2/download" | tar xj
WORKDIR /boost_1_63_0 
RUN ./bootstrap.sh --with-libraries=program_options,filesystem,system,regex,coroutine,context
RUN ./b2 cxxflags=-fPIC cflags=-fPIC link=static $BUILD_CORES_BOOST

# --------------------------------------------------------------------------------


# FreeImage
WORKDIR /
RUN wget http://downloads.sourceforge.net/freeimage/FreeImage3170.zip
RUN unzip FreeImage3170.zip
WORKDIR /FreeImage
RUN make $BUILD_CORES && make install

# --------------------------------------------------------------------------------

# Assimp
WORKDIR /
RUN git clone --depth=1 --branch v3.1.1 https://github.com/assimp/assimp.git assimp
WORKDIR /assimp/build
RUN cmake -DCMAKE_BUILD_TYPE=Release .. && make $BUILD_CORES && make install

# --------------------------------------------------------------------------------

# Embree
WORKDIR /
RUN git clone --depth=1 --branch v2.8.0 https://github.com/embree/embree.git embree
WORKDIR /embree/build
RUN cmake -D CMAKE_BUILD_TYPE=Release -D ENABLE_ISPC_SUPPORT=OFF -D RTCORE_TASKING_SYSTEM=INTERNAL -D ENABLE_TUTORIALS=OFF .. && make $BUILD_CORES && make install && cp libembree.so /usr/local/lib

# --------------------------------------------------------------------------------

# yaml-cpp
WORKDIR /
RUN git clone --depth=1 https://github.com/jbeder/yaml-cpp.git yaml-cpp
WORKDIR /yaml-cpp/build
RUN cmake -DCMAKE_BUILD_TYPE=Release -D BUILD_SHARED_LIBS=ON .. && make $BUILD_CORES && make install

# --------------------------------------------------------------------------------

# tbb
WORKDIR /
RUN git clone --depth=1 --branch 2017_U5 https://github.com/01org/tbb.git tbb
WORKDIR /tbb
RUN make $BUILD_CORES
RUN cp -rf build/linux_*_release/lib* /usr/local/lib/
RUN cp -rf include/tbb /usr/local/include/

# --------------------------------------------------------------------------------

USER $NB_USER

RUN pip install imageio
RUN conda install -c menpo -y opencv3=3.1.0

# --------------------------------------------------------------------------------

USER root
RUN apt-get install -qq -y openssh-client 

# --------------------------------------------------------------------------------

#USER root

# Add a project file to the container
#WORKDIR /home/$NB_USER/
#COPY . /home/$NB_USER/lightmetrica/

# Avois clock skew detected warning
#RUN find /home/$NB_USER/lightmetrica -print0 | xargs -0 touch

# Build lightmetrica
#WORKDIR /home/$NB_USER/lightmetrica/build
#RUN BOOST_ROOT="" BOOST_INCLUDEDIR="/boost_1_63_0" BOOST_LIBRARYDIR="/boost_1_63_0/stage/lib" cmake -DCMAKE_BUILD_TYPE=Release -D LM_USE_SINGLE_PRECISION=OFF -D LM_USE_DOUBLE_PRECISION=ON .. && make $BUILD_CORES

#ENV PATH /lightmetrica/dist/bin/Release:$PATH
#ENTRYPOINT ["lightmetrica"]

# --------------------------------------------------------------------------------

USER root
#COPY id_rsa /home/$NB_USER/.ssh/id_rsa
#RUN chown -R $NB_USER:users /home/$NB_USER/.ssh

WORKDIR /home/$NB_USER/work
USER $NB_USER
