ARG ENVIRONMENT=master
ARG BASE_IMAGE

# Conditional execution based on two base images: see https://stackoverflow.com/a/54245466
# Invoke with RSYSLOG_PREPOSITION set to either "with" or "without"
ARG RSYSLOG_PREPOSITION

# ##### Base image with just the PEP binaries

FROM ${BASE_IMAGE} AS base_without_rsyslog

# Vanilla BASE_IMAGE: don't change/add anything

# ##### Base image with (PEP binaries and) rsyslog integration

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
# 1. Creates a directory for logs with proper permissions
# 2. Sets up permissions for rsyslog
# 3. Starts rsyslog in the background with error handling, then runs the command passed to the container
RUN echo '#!/usr/bin/env bash\n\
mkdir -p /data/logs /var/log\n\
chmod -R 755 /data/logs /var/log\n\
touch /data/logs/pep.log /var/log/syslog 2>/dev/null || true\n\
chown -R syslog:adm /data/logs /var/log /data/logs/pep.log /var/log/syslog\n\
chmod 644 /data/logs/pep.log /var/log/syslog 2>/dev/null || true\n\
rsyslogd -n &\n\
RSYSLOG_PID=$!\n\
sleep 1\n\
if ! kill -0 $RSYSLOG_PID 2>/dev/null; then\n\
  echo "Warning: rsyslog failed to start properly."\n\
fi\n\
exec "$@"' > /entrypoint.sh && chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]

# ##### Target image adds configuration to base image "with" or "without" rsyslog

FROM base_${RSYSLOG_PREPOSITION}_rsyslog
ARG ENVIRONMENT
ARG PROJECT_DIR

# Set environment variable to find config
ENV PEP_CONFIG_DIR="/config"

# Combine config files from project and environment directories
COPY ${PROJECT_DIR}/client /config/
COPY ${ENVIRONMENT}/client ${ENVIRONMENT}/rootCA.cert ${ENVIRONMENT}/configVersion.json ${ENVIRONMENT}/ShadowAdministration.pub /config/
