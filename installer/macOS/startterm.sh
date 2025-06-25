#!/usr/bin/env bash
osascript -e "tell application \"Terminal\" to do script \"cd '$(dirname -- "$0")'; ./../Resources/runpepcli.sh\""