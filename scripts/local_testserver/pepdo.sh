#!/usr/bin/env bash

#SET CONFIGURATION VARIABLES (such as SCRIPTPATH and BUILDPATH)
# shellcheck disable=SC1091
source "testserver_settings.config" || exit

if [ "$TOKENSECRET" = true ] && [ -z "$TOKENGROUP" ]; then
  console_alert "ERROR: TOKENGROUP REQUIRED BUT NOT SET. (invoke pepaa.sh, pepda.sh or pepra.sh instead?)"
  exit
fi

if [ "$TOKENSECRET" = false ] && [ -n "$TOKENGROUP" ]; then
  console_alert "ERROR: TOKENSECRET = $TOKENSECRET, but tokengroup is provided (and would be ignored). Run pepdo.sh directly instead? "
  exit
fi


tryPaths=("${BUILDPATH}/cpp/pep/cli" "${BUILDPATH}/cli")
for tryPath in "${tryPaths[@]}"; do # Probing options where rootCA might be found:
  if [[ -f "$tryPath/pepcli" ]]; then # check if file exists at given path
    verbose "Found $tryPath/pepcli as a valid path for pepcli"
    CLIPATH="$tryPath" # set found loc
    break; #break for loop
  else 
    verbose "Did not find pepcli at $tryPath/pepcli"
  fi
done


if [ -z "$CLIPATH" ]; then
  console_alert "ERROR: couldn't find the pepcli binary in the build dir."
else 
  if  [ "$TOKENSECRET" = false ]  || [ ! -f "./OAuthTokenSecret.json" ]; then
    verbose "Not using oauth-token-secret since TOKENSECRET = $TOKENSECRET or OAuthTokenSecret was not found in cwd."
    "${CLIPATH}/pepcli" --client-config-name ClientConfig.json --client-working-directory . "$@"
  else 
    verbose "Token at ./OAuthTokenSecret.json"
    "${CLIPATH}/pepcli" --client-config-name ClientConfig.json --client-working-directory . --oauth-token-group "$TOKENGROUP" --oauth-token-secret "./OAuthTokenSecret.json" "$@"
  fi
fi

