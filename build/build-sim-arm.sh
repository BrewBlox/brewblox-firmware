#! /usr/bin/env bash
set -e

BUILD_DIR=$(dirname "$(readlink -f "$0")")
ROOT_DIR="${BUILD_DIR}/.."

# clean target dir to not have amd64 leftovers. Sudo needed because docker has created some files as privileged
sudo rm -rf "${BUILD_DIR}/target"

docker run \
    --rm \
    --privileged \
    docker/binfmt:a7996909642ee92942dcd6cff44b9b95f08dad64

# remove unsupported arg
sed -i 's/-m64//g' "${ROOT_DIR}/platform/spark/device-os/build/gcc-tools.mk"

# build
docker run \
    -it --rm \
    -v "${ROOT_DIR}":/firmware \
    brewblox/firmware-compiler:arm-latest \
    make APP=brewblox PLATFORM=gcc

# reset modified file
pushd "${ROOT_DIR}/platform/spark/device-os" > /dev/null
git checkout -- build/gcc-tools.mk 
popd > /dev/null
