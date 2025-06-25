#!/usr/bin/env bash

#PEP KEY GUARDS: Make sure OAuthTokenSecret and rootCA.cert are available.

#SET CONFIGURATION VARIABLES (such as SCRIPTPATH and BUILDPATH)
# shellcheck disable=SC1091
source "testserver_settings.config" || exit

TOKENFILE=OAuthTokenSecret.json
if [ ! -f "$TOKENFILE" ]; then
  echo "File $TOKENFILE does not exist."
  if [ -f "$BUILDPATH/../../ops/config/$TOKENFILE" ]; then
    while true; do
      read -r -p "Do you wish to link $TOKENFILE from ops to \"$(pwd)\"? [y/n]? " yn
      case $yn in
        [Yy]* ) ln -s "$BUILDPATH/../../ops/config/OAuthTokenSecret.json" .; break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
      esac
    done # end while loop
  fi # end if no source candidate for linking
fi # end if no tokenfile

# RootCA trouble? ln -s ../pki/rootCA.cert  in /cli/
ROOTCERTFILE="rootCA.cert"
if [ ! -f "$ROOTCERTFILE" ] ; then
  if [ -f "${BUILDPATH}/pki/$ROOTCERTFILE" ]; then
    while true; do
      read -r -p "Link \"${BUILDPATH}/pki/$ROOTCERTFILE\" to \"$(pwd)\"? [y/n]? " yn
      case $yn in
        [Yy]* ) ln -s "${BUILDPATH}/pki/$ROOTCERTFILE" .; break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
      esac
    done # end while loop
  else  # else if no source candidate for linking
    echo "Cannot link rootCA.cert in working dir, file doesn't exist at ${BUILDPATH}/pki/$ROOTCERTFILE";
  fi
fi # end if no rootcertfile

# In addition, add "ln -s ../../../../ops/keys/CastorAPIKey.json" to core/build/servers/keyserver
