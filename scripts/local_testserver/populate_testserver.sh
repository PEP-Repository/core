#!/usr/bin/env bash

#SET CONFIGURATION VARIABLES (such as SCRIPTPATH and BUILDPATH)
# shellcheck disable=SC1091
source "testserver_settings.config" || exit

if [ -n "$SERVERPID_FILENAME" ] && [ -f "$SERVERPID_FILENAME" ]; then
  echo "WARNING: SERVERPID file ($SERVERPID_FILENAME) exists, suggests a server is already running. Please stop server or remove PID file before starting a new server."
  exit
fi

SERVERBIN="$BUILDPATH/cpp/pep/servers/pepServers"
if [ ! -f "$SERVERBIN" ]; then
    console_alert "pepServers binary not present at '$SERVERBIN'. Please make sure PEP is built and this script is called from the binary/build directory."
    exit
fi

# Stash current working dir to return later
cwd=$(pwd)

# Make sure server has CastorAPIKey (uncomment line when not properly configured by cmake)
# ln -s ../../../../ops/keys/CastorAPIKey.json ../../build/servers/registrationserver/

# Make sure clients may use rootCA and OAuth files (create links when absent and found at expected locations)
"$SCRIPTPATH/check_keylinks.sh"

# Call servers in the background
cd "$BUILDPATH/cpp/pep/servers/" || exit
./pepServers &

SERVERPID="$!"

# A script termination routine to be called upon a forced shutdown.
function terminate_routine {
  if [ "$1" = "Clean" ]; then
    console_update "Clean shutdown routine invoked."
  else
    console_alert "Forcing shutdown routine after EXIT SIGHUP SIGINT or SIGTERM."
  fi

  if [ -n "$SERVERPID" ]; then # Check whether a server should be running
    console_update "Revoking Data Admin rights on * ."
    cd "$cwd" || exit
    "$SCRIPTPATH/pepaa.sh" ama cgar remove \* "Data Administrator" read
    "$SCRIPTPATH/pepaa.sh" ama cgar remove \* "Data Administrator" write
    console_update "Shutting down pepServers ($SERVERPID)"
    kill "$SERVERPID"
    SERVERPID=""
    console_update "Shutdown ready."
    exit
  else 
    console_update "Repeated process shutdown routine called. Servers had already been shut down. Happy testing."
    exit
  fi
}
trap terminate_routine SIGHUP SIGINT SIGTERM


console_update "Server PID = ${SERVERPID}. Now sleep for 2s to give the server a headstart."
sleep 2s

console_update "Return back to cwd: ${cwd} and temporarily granting DA read an write access to * ."
cd "$cwd" || exit

# Give Data Admin permission to write to all columns using Access Admin
"$SCRIPTPATH/pepaa.sh" ama cgar create \* "Data Administrator" read
"$SCRIPTPATH/pepaa.sh" ama cgar create \* "Data Administrator" write

# Create 10 test participants using Data Adminb
console_update "Registering 10 test participants"
"$SCRIPTPATH/pepda.sh" register test-participants --number 10

# This is the likely place for future extension of the test-data.
#
# [ Work in progress ]
#


console_update "Showing what's on the server:"
"$SCRIPTPATH/pepda.sh" list -C \* -P \* 

console_update "Cleanup ClientKeys.json; OAuthToken.json; rotated_logs/ ..."
rm -rf ClientKeys.json OAuthToken.json rotated_logs/

# Explicit/Clean shutdown: revoke DA rights, kill pep servers.
terminate_routine Clean
