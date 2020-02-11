FROM ubuntu:18.04

ENV TZ=Europe/Amsterdam

# for apt to be noninteractive
ENV DEBIAN_FRONTEND noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN true

# download google protobuf source files (used in test code) and install them as system includes.
# this is the slowest step, so we do this first so it is more likely to cache this layer
RUN apt-get update -q \
  && apt-get install -qy curl \
  && apt-get install -qy --no-install-recommends build-essential autoconf automake libtool  unzip pkg-config \
  && curl -sSL https://github.com/protocolbuffers/protobuf/releases/download/v3.6.1/protobuf-cpp-3.6.1.zip -o /tmp/protobuf.zip \
  && unzip /tmp/protobuf.zip -d /tmp/protobuf \
  && cd /tmp/protobuf/protobuf-3.6.1 \
  && ./configure \
  && make \
  && make check \
  && make install \
  && ldconfig \
  && apt-get clean \
  && rm -rf /tmp/* /var/tmp/*

# install arm compiler
ENV GCC_ARM_URL="https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/8-2019q3/RC1.1/gcc-arm-none-eabi-8-2019-q3-update-linux.tar.bz2"
ENV GCC_ARM_VERSION="8-2019-q3-update"

RUN dpkg --add-architecture i386 \
  && apt-get update -q \
  && apt-get install --no-install-recommends -qy isomd5sum bzip2 vim-common libarchive-zip-perl libc6:i386 \
  && curl -o /tmp/gcc-arm-none-eabi.tar.bz2 -sSL ${GCC_ARM_URL} \
  && tar xjvf /tmp/gcc-arm-none-eabi.tar.bz2 -C /usr/local \
  && mv /usr/local/gcc-arm-none-eabi-${GCC_ARM_VERSION}/ /usr/local/gcc-arm-embedded \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /usr/local/gcc-arm-embedded/share

ENV PATH /usr/local/gcc-arm-embedded/bin:$PATH

# preesed tzdata to prevent interactive install prompt
RUN echo "tzdata tzdata/Areas select Europe" > /tmp/preseed.txt; \
  echo "tzdata tzdata/Zones/Europe select Amsterdam" >> /tmp/preseed.txt; \
  debconf-set-selections /tmp/preseed.txt

# install various packages needed for the build, testing, or for building the dependencies
RUN apt-get update -q \
  && apt-get install -qy --no-install-recommends tzdata git bash dfu-util python python-pip gdb lcov rsync colormake \ 
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# set timezone
RUN rm -rf /etc/timezone \
  && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone \
  && dpkg-reconfigure --frontend noninteractive tzdata

# and install protobuf python package (used by nanopb), and pyserial (used by program-dfu)
RUN pip install --upgrade pip setuptools
RUN pip install protobuf pyserial compiledb

# download and compile boost and install as system includes (/usr/local)
ENV BOOST_VERSION=1_59_0
ENV BOOST_ROOT=/boost/boost_$BOOST_VERSION

RUN mkdir -p /boost && curl -sSL https://s3.amazonaws.com/spark-assets/boost_$BOOST_VERSION.tar.gz | tar -xz -C /boost \
  && export DYLD_LIBRARY_PATH="$BOOST_ROOT/stage/lib:$DYLD_LIBRARY_PATH" \
  && export LD_LIBRARY_PATH="$BOOST_ROOT/stage/lib:$LD_LIBRARY_PATH" \
  && cd $BOOST_ROOT \
  && ./bootstrap.sh \  
  && ./b2  --with-thread  --with-system --with-program_options --with-random --with-regex --threading=multi

WORKDIR /firmware/build
CMD bash
