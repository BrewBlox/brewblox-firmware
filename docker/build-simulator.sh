#! /usr/bin/env bash
set -e

docker-compose exec -T compiler \
    bash -c "
        set -e
        rm -rf ../docker/simulator/target
        bash compile-proto.sh
        make $MAKE_ARGS APP=brewblox PLATFORM=gcc
        cp -r target ../docker/simulator/target
    "

docker build --no-cache -t brewblox/firmware-simulator:local simulator
