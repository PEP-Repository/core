ARG ENVIRONMENT=main
ARG BASE_IMAGE

# Conditional execution based on two base images: see https://stackoverflow.com/a/54245466
# Invoke with RSYSLOG_PREPOSITION set to either "with" or "without"
ARG RSYSLOG_PREPOSITION

##### Base image with just the PEP binaries

FROM ${BASE_IMAGE} AS base_without_rsyslog

# Vanilla BASE_IMAGE: don't change/add anything

##### Base image with (PEP binaries and) rsyslog integration

FROM ${BASE_IMAGE} AS base_with_rsyslog

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends rsyslog rsyslog-gnutls logrotate && \
    apt-get clean && rm -rf /var/cache/* /var/lib/{apt,dpkg,cache,log}/* /tmp/* /var/tmp/*

COPY rsyslog/rsyslog.d /etc/rsyslog.d
COPY rsyslog/logrotate.d /etc/logrotate.d

# Disable kernel log module which doesn't work in containers which
# dont run with --privileged flag. Disables by commenting out either:
# module(load="imklog")
# module(load="imklog" permitnonkernelfacility="on")
RUN sed -i '/imklog/s/^/#/' /etc/rsyslog.conf

# Create entrypoint script that:
# 1. Sets up a single place for logs, creating links so partners don't need to change their configuration (for ppp and hb, not for self hosted envs).
# 2. For each service, link their logs dir to /data/logs, as they have their data directory mounted to a different location.
# 3. Sets up read and write permissions for syslog, and read for others, 755 (rwxr-xr-x) is required for the directory, as it needs the execute bit set to be traversable.
# 3. Starts rsyslog and cron in the background, non-daemonized, so the processes can be retrieved for graceful shutdown, also waits and kills the foreground process.
# 4. Runs the main container command passed as arguments ("$@").
# 5. Cleans up background services when the main command exits or a signal is received.
RUN echo '#!/usr/bin/env bash\n\
set -eu\n\
for datadir in accessmanager keyserver registrationserver storagefacility transcryptor; do\n\
  if [ -d "/data/$datadir" ]; then\n\
    mkdir -p /data/$datadir/logs\n\
    chmod 777 /data/$datadir/logs\n\
    ln -s /data/$datadir/logs /data/logs\n\
    break\n\
  fi\n\
done\n\
if [[ ! -d "/data/logs" ]]; then\n\
  mkdir -p /data/logs\n\
  chmod 755 /data/logs\n\
  touch /data/logs/pep.log\n\
  chown syslog:adm /data/logs /data/logs/pep.log\n\
  chmod 644 /data/logs/pep.log\n\
fi\n\
stop_services() {\n\
  kill $(jobs -rp)\n\
  wait -f $(jobs -rp) || true\n\
}\n\
trap stop_services EXIT\n\
rsyslogd -n &\n\
cron -f &\n\
"$@" &\n\
wait -f %' > /entrypoint.sh && chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]

##### Target image adds configuration to base image "with" or "without" rsyslog

FROM base_${RSYSLOG_PREPOSITION}_rsyslog
ARG ENVIRONMENT
ARG PROJECT_DIR

# We first copy ${PROJECT_DIR}, and then the other files, so that the second step may overwrite files in ${PROJECT_DIR}
COPY ${PROJECT_DIR}/ /config/
COPY ${ENVIRONMENT}/pep-services ${ENVIRONMENT}/rootCA.cert ${ENVIRONMENT}/configVersion.json ${ENVIRONMENT}/ShadowAdministration.pub /config/

# Hard-link configVersion.json into subdirectories, placing copies next to all server config files.
RUN find /config -type d ! -wholename /config -exec ln /config/configVersion.json {}/configVersion.json \;
