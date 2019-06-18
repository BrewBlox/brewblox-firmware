#!/bin/bash
MY_DIR=$(dirname "$(readlink -f "$0")")

handle_error() {
  echo "Encountered error when executing $(basename $0)!" >&2
  echo "Error on line $(caller)" >&2
  exit 1
}
trap handle_error ERR

pushd "$MY_DIR/../app/brewblox/proto" > /dev/null
echo -e "Compiling proto files using nanopb for brewblox firmware"
bash generate_proto_cpp.sh;

echo -e "Compiling proto files using google protobuf for unit tests"
bash generate_proto_test_cpp.sh;
popd > /dev/null

echo "Done"
exit