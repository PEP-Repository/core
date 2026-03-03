ARG BASE_IMAGE
FROM ${BASE_IMAGE}

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends cron && \
    apt-get clean && rm -rf /var/cache/* /var/lib/{apt,dpkg,cache,log}/* /tmp/* /var/tmp/*

# Generic entrypoint supporting both oneshot and cron modes
# Environment variables:
# - MODE: 'oneshot' (default) or 'cron'  
# - CRON_SCHEDULE: cron schedule format (default: "0 * * * *")
# - COMMAND: command to execute
# To manually trigger in cron mode: docker exec <container> /run-now.sh

# To keep docker WORKDIR and runtime WORK_DIR consistent
ARG WORK_DIR=/data
ENV WORK_DIR=${WORK_DIR}

# Helper script for manual execution, while in cron mode
RUN echo '#!/usr/bin/env bash\n\
if [ -z "$COMMAND" ]; then\n\
    echo "Error: COMMAND environment variable is not set"\n\
    exit 1\n\
fi\n\
echo "Manually executing: $COMMAND"\n\
cd "$WORK_DIR"\n\
exec $COMMAND "$@" > /proc/1/fd/1 2>/proc/1/fd/2' > /run-now.sh && chmod +x /run-now.sh

RUN echo '#!/usr/bin/env bash\n\
set -e\n\
\n\
MODE="${MODE:-oneshot}"\n\
CRON_SCHEDULE="${CRON_SCHEDULE:-0 * * * *}"\n\
COMMAND="${COMMAND}"\n\
\n\
if [ -z "$COMMAND" ]; then\n\
    echo "Error: COMMAND environment variable is not set"\n\
    exit 1\n\
fi\n\
\n\
case "$MODE" in\n\
    "cron")\n\
        echo "Starting in persistent cron mode with schedule: $CRON_SCHEDULE, and command: $COMMAND"\n\
        echo "$CRON_SCHEDULE root cd $WORK_DIR && PEP_CONFIG_DIR=$PEP_CONFIG_DIR PEP_LOGON_LIMITED=$PEP_LOGON_LIMITED $COMMAND $* > /proc/1/fd/1 2>/proc/1/fd/2" > /etc/cron.d/pep-scheduler\n\
        chmod 0644 /etc/cron.d/pep-scheduler\n\
        exec cron -f -L 15\n\
        ;;\n\
    "oneshot")\n\
        echo "Running in oneshot mode with command: $COMMAND"\n\
        exec $COMMAND "$@" > /proc/1/fd/1 2>/proc/1/fd/2\n\
        ;;\n\
    *)\n\
        echo "Error: Unknown MODE: $MODE (expected cron or oneshot)"\n\
        exit 1\n\
        ;;\n\
esac' > /entrypoint.sh && chmod +x /entrypoint.sh

WORKDIR ${WORK_DIR}
ENTRYPOINT ["/entrypoint.sh"]