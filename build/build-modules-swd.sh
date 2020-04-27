#!/bin/bash
MY_DIR=$(dirname $(readlink -f $0))

function status()
{
if [[ "$1" -eq 0 ]]; then
  echo "✓ SUCCESS"
else
  echo "✗ FAILED"
fi
}

pushd "$MY_DIR/../platform/spark/device-os/modules" > /dev/null

echo "Building system modules for P1 with SWD"
make -s all PLATFORM=p1 PARTICLE_DEVELOP=y USE_SWD=y
(( result = $? ))
status $result
(( exit_status = exit_status || result ))

echo "Flashing system modules with SWD"
make -s program-dfu PLATFORM=p1 PARTICLE_DEVELOP=y USE_SWD=y
(( result = $? ))
status $result
(( exit_status = exit_status || result ))

popd > /dev/null

exit $exit_status