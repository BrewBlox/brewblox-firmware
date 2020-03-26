#! /usr/bin/env bash
set -e

# Enables experimental features for both the Docker daemon and the Docker CLI

# Configuration files
CLIENT_CFG="$HOME/.docker/config.json"
SERVER_CFG="/etc/docker/daemon.json"

# Ensure "jq" is installed.
command -v jq > /dev/null || sudo apt-get install -y jq

CLIENT_EXP=$(docker version --format '{{.Client.Experimental}}')
if [[ "$CLIENT_EXP" != "true" ]]; then

    if [ ! -f "$CLIENT_CFG" ]; then
        echo "Creating $CLIENT_CFG"
        mkdir -p "$(dirname "$CLIENT_CFG")"
        echo '{}' > "$CLIENT_CFG"
    fi;

    echo "Updating $CLIENT_CFG"
    OUTPUT=$(jq '. + {"experimental":"enabled"}' "$CLIENT_CFG")
    echo "$OUTPUT" > "$CLIENT_CFG"
fi;

SERVER_EXP=$(docker version --format '{{.Server.Experimental}}')
if [[ "$SERVER_EXP" != "true" ]]; then

    if [ ! -f "$SERVER_CFG" ]; then
        echo "Creating $SERVER_CFG"
        sudo mkdir -p "$(dirname "$SERVER_CFG")"
        echo '{}' | sudo tee "$SERVER_CFG"
    fi;

    echo "Updating $SERVER_CFG"
    OUTPUT=$(sudo jq '. + {"experimental":true}' "$SERVER_CFG")
    echo "$OUTPUT" | sudo tee "$SERVER_CFG"
    sudo service docker restart
fi;
