#! /usr/bin/env bash

# The compiler image is expected to remain relatively stable
# It does not contain any firmware code - just the software required to compile the firmware

docker build -t brewblox/firmware-compiler:local compiler
