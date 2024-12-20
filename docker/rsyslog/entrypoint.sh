#!/usr/bin/env bash

#This shell script serves as an entry point for Docker images that send their logging thru rsyslog.

#We want to have a single place to log to, without having to change anything at the partners, even though each service has its data directory mounted to a different location
for datadir in accessmanager keyserver registrationserver storagefacility transcryptor; do
  if [ -d "/data/$datadir" ]; then
    mkdir -p /data/$datadir/logs
    chmod 777 /data/$datadir/logs
    ln -s /data/$datadir/logs /data/logs
    break
  fi
done

rsyslogd && cron && "$@"; sleep 5
