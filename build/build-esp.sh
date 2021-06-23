#! /usr/bin/env bash
set -ex

# Use repository root
pushd "$(dirname "$0")/.." > /dev/null

# clean target dir
sudo rm -rf build/target

# Compile proto
docker run \
    -it --rm \
    --pull always \
    -v "$PWD":/firmware \
    -w /firmware/build \
    brewblox/simulator-compiler:latest \
    bash compile-proto.sh

# Build with idf.py in a container
bash build/idf.sh build
