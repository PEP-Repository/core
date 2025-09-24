#!/usr/bin/env bash

#SET CONFIGURATION VARIABLES (such as SCRIPTPATH and BUILDPATH)
# shellcheck disable=SC1091
source "testserver_settings.config" || exit

tryPaths=("${BUILDPATH}/cpp/pep/logon" "${BUILDPATH}/apps")
for tryPath in "${tryPaths[@]}"; do # Probing options where rootCA might be found:
  if [[ -f "$tryPath/pepLogon" ]]; then # check if file exists at given path
    LOGONPATH="$tryPath" # set found loc
    break; #break for loop
  else
    verbose "pepLogon not in $tryPath."
  fi
done

if [ -z "$LOGONPATH" ]; then
  console_alert "ERROR: couldn't find the pepLogon binary in the build dir, this makes dynamic logging in rather difficult."
else 
  "${LOGONPATH}/pepLogon" "$@"
fi