FROM multiarch/ubuntu-core:armhf-bionic

ENV DEBIAN_FRONTEND=noninteractive \
  DEBCONF_NONINTERACTIVE_SEEN=true

# install various build deps
RUN apt-get update -q \
  && apt-get install -qy --no-install-recommends build-essential autoconf automake libtool unzip pkg-config libprotobuf-dev protobuf-compiler git bash python python-pip rsync curl \
  && pip install --upgrade pip setuptools \
  # and install protobuf python package (used by nanopb), and pyserial (used by program-dfu)
  && pip install protobuf \
  # crc32 is not used, build particle's makefile does a 'which crc32', this dummy avoids an error
  && touch /usr/bin/crc32 \
  && chmod +x /usr/bin/crc32 \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

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
