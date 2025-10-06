ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ARG STAGING_DIRECTORY=build

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends python3-pip cron && \
    apt-get clean && rm -rf /var/cache/* /var/lib/{apt,dpkg,cache,log}/* /tmp/* /var/tmp/*

# Install the wheel with a bind mount
RUN --mount=type=bind,source=${STAGING_DIRECTORY}/python/pep/pep_connector/,target=/tmp/pep_connector \
    pip install --break-system-packages --no-cache-dir /tmp/pep_connector/*.whl

# Create entrypoint script that allows the same image to be used as oneshot (with i.e. systemd timers, scripts, external cron jobs, etc.) or persistent with cron (i.e. for docker-compose)
# Configuration is done via environment variables to keep all settings in docker-compose.yml
# - MODE: 'oneshot' (default, run script once and exit) or 'cron' (persistent with scheduling)
# - CRON_SCHEDULE: cron schedule format for persistent mode (defaults to once every hour on the hour)
# - SCRIPT_PATH: path to the Python script to execute

RUN echo '#!/usr/bin/env bash\n\
set -e\n\
\n\
MODE="${MODE:-oneshot}"\n\
CRON_SCHEDULE="${CRON_SCHEDULE:-0 * * * *}"\n\
SCRIPT_PATH="${SCRIPT_PATH}"\n\
\n\
if [ -z "$SCRIPT_PATH" ]; then\n\
    echo "Error: SCRIPT_PATH environment variable is not set"\n\
    exit 1\n\
fi\n\
\n\
case "$MODE" in\n\
    "cron")\n\
        echo "Starting in persistent cron mode with schedule: $CRON_SCHEDULE, and script: $SCRIPT_PATH"\n\
        WORK_DIR=$(pwd)\n\
        echo "$CRON_SCHEDULE root cd $WORK_DIR && PEP_CONFIG_DIR=$PEP_CONFIG_DIR PEP_LOGON_LIMITED=$PEP_LOGON_LIMITED python3 $SCRIPT_PATH $* > /proc/1/fd/1 2>/proc/1/fd/2" > /etc/cron.d/pep-connector\n\
        chmod 0644 /etc/cron.d/pep-connector\n\
        exec cron -f -L 15\n\
        ;;\n\
    "oneshot")\n\
        echo "Running in oneshot mode"\n\
        exec python3 "$SCRIPT_PATH" "$@"\n\
        ;;\n\
    *)\n\
        echo "Error: Unknown MODE: $MODE (expected cron or oneshot)"\n\
        exit 1\n\
        ;;\n\
esac' > /entrypoint.sh && chmod +x /entrypoint.sh

WORKDIR /data

ENTRYPOINT ["/entrypoint.sh"]
