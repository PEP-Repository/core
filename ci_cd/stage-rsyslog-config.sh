#!/usr/bin/env sh

set -o errexit
set -o nounset

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

src="$1"
dest="$2"
proj="$3"

# Create destination (sub)directory
mkdir "$dest"/rsyslog

# Copy (client) configuration files from source to destination
cp -r "$src"/client/* "$dest"/rsyslog/

# Copy PKI files from source to destination
cp "$src"/rsyslogCA.chain             "$dest"/rsyslog/rsyslog.d/
cp "$src"/RSysLogrsyslog-client.chain "$dest"/rsyslog/rsyslog.d/
cp "$src"/RSysLogrsyslog-client.key   "$dest"/rsyslog/rsyslog.d/

# Copy "entrypoint.sh" script from FOSS repo to destination
cp "$SCRIPTPATH"/../docker/rsyslog/entrypoint.sh "$dest"/rsyslog/

# Create config file with project abbreviation
abbrev=$(cat "$proj"/caption.txt)
echo "set $.projectname = '$abbrev';" > "$dest"/rsyslog/rsyslog.d/10-pep-projectname.conf
