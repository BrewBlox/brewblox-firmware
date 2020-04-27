#!/bin/bash
MY_DIR=$(dirname $(readlink -f $0))
BUILD_DIR="$MY_DIR/target/user/platform-3/firmware/brewblox"
EXECUTABLE_DIR="$MY_DIR/target/brewblox-gcc"
EXECUTABLE="$EXECUTABLE_DIR/brewblox"
OUTPUT_DIR="$MY_DIR/coverage"
DEVICE_KEY="$EXECUTABLE_DIR/device_key.der"
SERVER_KEY="$EXECUTABLE_DIR/server_key.der"
STATE_DIR="$EXECUTABLE_DIR/state"
EEPROM_FILE="$EXECUTABLE_DIR/eeprom.bin"

ls "$EXECUTABLE" 
if [ ! -f "$EXECUTABLE" ]; then
    echo "brewblox executable not found!"
    exit 1
fi

pushd "$EXECUTABLE_DIR" || exit

# eeprom file writes cause a lot of memory allocation. Import blocks manually after start for a less polluted
touch "$DEVICE_KEY" "$SERVER_KEY" "$EEPROM_FILE"
mkdir -p "$STATE_DIR"
mkdir -p "$OUTPUT_DIR"

rm "$EXECUTABLE_DIR/massif.out"
rm "$EXECUTABLE_DIR/xtmemory.kcg"

valgrind --tool=massif --threshold=0.1 \
--xtree-memory=full --xtree-memory-file="$EXECUTABLE_DIR/xtmemory.kcg" \
--ignore-fn=call_init.part.0 \
--ignore-fn=_IO_file_doallocate \
--massif-out-file="$EXECUTABLE_DIR/massif.out" "$EXECUTABLE" --device_id 123456789012345678901234 --device_key="$DEVICE_KEY" --server_key="$SERVER_KEY"

popd || exit

# open massif.out with massif-visualizer
# massif-visualizer "$EXECUTABLE_DIR/massif.out" &
# open xtmemory.kcg with kcachegrind
kcachegrind "$EXECUTABLE_DIR/xtmemory.kcg" 