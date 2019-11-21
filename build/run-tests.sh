#!/bin/bash
MY_DIR=$(dirname "$(readlink -f "$0")")

function status()
{
if [[ "$1" -eq 0 ]]; then
  echo "✓ SUCCESS"
else
  echo "✗ FAILED"
fi
}

echo "Running lib unit tests"
pushd "$MY_DIR/../lib/test/build" > /dev/null
./lib_test_runner --durations yes;
(( result = $? ))
status $result
(( exit_status = exit_status || result ))
popd > /dev/null

echo "Running ControlBox unit tests"
pushd "$MY_DIR/../controlbox/build/" > /dev/null
./cbox_test_runner --durations yes;
(( result = $? ))
status $result
(( exit_status = exit_status || result ))
popd > /dev/null


echo "Running BrewBlox unit tests"
pushd "$MY_DIR/../app/brewblox/test/build" > /dev/null
./brewblox_test_runner --durations yes;
(( result = $? ))
status $result
(( exit_status = exit_status || result ))
popd > /dev/null

exit $exit_status
