#!/usr/bin/env bash
osascript -e "tell application \"Terminal\"" \
          -e "do script \"cd '$(dirname -- "$0")'; ./../Resources/runpepcli.sh\"" \
          -e "activate" \
          -e "end tell"