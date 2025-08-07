#!/usr/bin/env bash

#SET CONFIGURATION VARIABLES (such as SCRIPTPATH and BUILDPATH)
# shellcheck disable=SC1091
source "testserver_settings.config" || exit


if [ -f "$SERVERPID_FILENAME" ]; then
  # shellcheck disable=SC1090
  source "$SERVERPID_FILENAME"
  kill "$SERVERPID"
  echo "Killed server";
  rm "$SERVERPID_FILENAME"
else 
  echo "No server-pid found. Not shutting down anything."
fi