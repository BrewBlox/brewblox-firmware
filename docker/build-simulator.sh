#! /usr/bin/env bash
set -e

docker-compose exec compiler \
    bash -c '
        set -e        
        rm -rf ../docker/simulator/target
        bash compile-proto.sh
        make -j APP=brewblox PLATFORM=gcc
        cp -r target ../docker/simulator/target
    '

docker build -t brewblox/firmware-simulator:local simulator
