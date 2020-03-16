FROM ubuntu:19.10

ENV TZ=Europe/Amsterdam \
  DEBIAN_FRONTEND=noninteractive \
  DEBCONF_NONINTERACTIVE_SEEN=true

# install various build deps
RUN apt-get update -q \
  && apt-get install -qy curl \
  && apt-get install -qy --no-install-recommends build-essential autoconf automake libtool unzip pkg-config libprotobuf-dev protobuf-compiler tzdata git bash dfu-util python python-pip gdb lcov rsync colormake \
  && pip install --upgrade pip setuptools \
  # and install protobuf python package (used by nanopb), and pyserial (used by program-dfu)
  && pip install protobuf pyserial compiledb \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# install arm compiler
ENV GCC_ARM_URL="https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/8-2019q3/RC1.1/gcc-arm-none-eabi-8-2019-q3-update-linux.tar.bz2" \
  GCC_ARM_VERSION="8-2019-q3-update"

RUN dpkg --add-architecture i386 \
  && apt-get update -q \
  && apt-get install --no-install-recommends -qy isomd5sum bzip2 vim-common libarchive-zip-perl libc6:i386 \
  && curl -o /tmp/gcc-arm-none-eabi.tar.bz2 -sSL ${GCC_ARM_URL} \
  && tar xjvf /tmp/gcc-arm-none-eabi.tar.bz2 -C /usr/local \
  && mv /usr/local/gcc-arm-none-eabi-${GCC_ARM_VERSION}/ /usr/local/gcc-arm-embedded \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /usr/local/gcc-arm-embedded/share

ENV PATH /usr/local/gcc-arm-embedded/bin:$PATH

# preesed tzdata to prevent interactive install prompt and then set timezone
RUN echo "tzdata tzdata/Areas select Europe" > /tmp/preseed.txt; \
  echo "tzdata tzdata/Zones/Europe select Amsterdam" >> /tmp/preseed.txt; \
  debconf-set-selections /tmp/preseed.txt; \
  rm -rf /etc/timezone \
  && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone \
  && dpkg-reconfigure --frontend noninteractive tzdata

# download and compile boost and install as system includes (/usr/local)
ENV BOOST_VERSION=1_72_0 \
  BOOST_ROOT=/boost/boost_1_72_0

RUN mkdir -p /boost && curl -sSL https://s3.amazonaws.com/spark-assets/boost_$BOOST_VERSION.tar.gz | tar -xz -C /boost \
  && export DYLD_LIBRARY_PATH="$BOOST_ROOT/stage/lib:$DYLD_LIBRARY_PATH" \
  && export LD_LIBRARY_PATH="$BOOST_ROOT/stage/lib:$LD_LIBRARY_PATH" \
  && cd $BOOST_ROOT \
  && ./bootstrap.sh \  
  && ./b2  --with-thread --with-system --with-program_options --with-random --with-regex --threading=single link=static runtime-link=static

WORKDIR /firmware/build
CMD bash
