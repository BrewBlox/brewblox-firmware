#! /usr/bin/env bash
set -e

docker run \
    --rm \
    --volume $(pwd)/../:/firmware \
    --name flasher-compiler \
    brewblox/firmware-compiler \
    bash -c '
        set -e
        cd /firmware/build
        rm -rf /firmware/docker/simulator/target
        bash compile-proto.sh
        make clean APP=brewblox PLATFORM=p1
        make -j APP=brewblox PLATFORM=p1
        cp /firmware/build/target/brewblox-p1/brewblox.bin /firmware/docker/flasher/brewblox-p1.bin
        make clean APP=brewblox PLATFORM=photon
        make -j APP=brewblox PLATFORM=photon
        cp /firmware/build/target/brewblox-photon/brewblox.bin /firmware/docker/flasher/brewblox-photon.bin
    '

docker run --rm --privileged multiarch/qemu-user-static:register --reset

docker build --no-cache -f flasher/amd/Dockerfile -t brewblox/firmware-flasher:local flasher
docker build --no-cache -f flasher/arm/Dockerfile -t brewblox/firmware-flasher:rpi-local flasher
