#!/usr/bin/env sh

set -eu


# this script is run as the www-data user, which does not have the privileges to reload apache.
# So instead we touch a file, which is monitored by the md-monitor.sh script, which does have the correct privileges
touch /var/md-notify/md-notify