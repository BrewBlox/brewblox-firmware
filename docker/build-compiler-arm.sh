#!/bin/bash

docker run --rm --privileged docker/binfmt:a7996909642ee92942dcd6cff44b9b95f08dad64
docker build --platform linux/arm -t brewblox/firmware-compiler-arm:local compiler-arm
