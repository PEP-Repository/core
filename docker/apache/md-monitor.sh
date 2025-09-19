#!/usr/bin/env sh
# When this domain is signed up or renewed with let's encrypt, a script is triggered as the (non-privileged) www-data user wich touched the file md-notify. This script picks up that change to restart apache as a privileged user.
set -eu

while :; do
  inotifywait -e attrib /var/md-notify/md-notify
  apachectl -k graceful
done