#!/usr/bin/env bash

#SET CONFIGURATION VARIABLES (such as SCRIPTPATH and BUILDPATH)
# shellcheck disable=SC1091
source "testserver_settings.config" || exit

SERVERSPATH="${BUILDPATH}/cpp/pep/servers"

cd "$SERVERSPATH" || exit

./pepServers