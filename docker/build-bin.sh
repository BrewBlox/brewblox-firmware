#! /usr/bin/env bash
set -e

PARTICLE_TAG=$(git --git-dir ../platform/spark/device-os/.git describe --tags)
PARTICLE_RELEASES=https://github.com/particle-iot/device-os/releases/download/${PARTICLE_TAG}
PARTICLE_VERSION=${PARTICLE_TAG:1} # remove the 'v' prefix

MY_DIR=$(dirname $(readlink -f $0))
OUT_DIR="./firmware-bin/source"

rm -rf "${OUT_DIR}" || true
mkdir "${OUT_DIR}"

if [ -z "$SKIP_BUILD" ]
then
    docker-compose exec -T compiler \
        bash -c "
            set -e
            bash compile-proto.sh
            make $MAKE_ARGS APP=brewblox PLATFORM=p1
            make $MAKE_ARGS APP=brewblox PLATFORM=photon
        "
fi

cp ../build/target/brewblox-p1/brewblox.bin "${OUT_DIR}"/brewblox-p1.bin
curl -fL -o "${OUT_DIR}"/bootloader-p1.bin ${PARTICLE_RELEASES}/p1-bootloader@${PARTICLE_VERSION}+lto.bin
curl -fL -o "${OUT_DIR}"/system-part1-p1.bin ${PARTICLE_RELEASES}/p1-system-part1@${PARTICLE_VERSION}.bin
curl -fL -o "${OUT_DIR}"/system-part2-p1.bin ${PARTICLE_RELEASES}/p1-system-part2@${PARTICLE_VERSION}.bin

cp ../build/target/brewblox-photon/brewblox.bin "${OUT_DIR}"/brewblox-photon.bin
curl -fL -o "${OUT_DIR}"/bootloader-photon.bin ${PARTICLE_RELEASES}/photon-bootloader@${PARTICLE_VERSION}+lto.bin
curl -fL -o "${OUT_DIR}"/system-part1-photon.bin ${PARTICLE_RELEASES}/photon-system-part1@${PARTICLE_VERSION}.bin
curl -fL -o "${OUT_DIR}"/system-part2-photon.bin ${PARTICLE_RELEASES}/photon-system-part2@${PARTICLE_VERSION}.bin

FILE="${OUT_DIR}"/firmware.ini
touch $FILE

echo "[FIRMWARE]" >> $FILE
echo "firmware_version=$(git rev-parse --short HEAD)" >> $FILE
echo "firmware_date=$(git show -s --format=%ci)" >> $FILE
echo "proto_version=$(cd $MY_DIR/../app/brewblox/proto; git rev-parse --short HEAD)" >> $FILE
echo "proto_date=$(cd $MY_DIR/../app/brewblox/proto; git show -s --format=%ci)" >> $FILE
echo "system_version=${PARTICLE_VERSION}" >> $FILE

docker build --no-cache -t brewblox/firmware-bin:local firmware-bin
