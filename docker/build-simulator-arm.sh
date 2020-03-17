#! /usr/bin/env bash
set -e

docker run --rm --privileged docker/binfmt:a7996909642ee92942dcd6cff44b9b95f08dad64

rm -rf ../docker/simulator-arm/target
sed -i 's/-m64//g' ../platform/spark/device-os/build/gcc-tools.mk # remove unsupported arg
docker run -v "$(pwd)"/../:/firmware/ brewblox/firmware-compiler-arm:develop make APP=brewblox PLATFORM=gcc
pushd ../platform/spark/device-os > /dev/null
git checkout -- build/gcc-tools.mk # reset modified file
popd > /dev/null
cp -r ../build/target simulator-arm/target

docker build --no-cache --platform linux/arm -t brewblox/firmware-simulator-arm:local simulator-arm
