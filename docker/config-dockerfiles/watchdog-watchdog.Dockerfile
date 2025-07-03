ARG ENVIRONMENT=master
ARG BASE_IMAGE
FROM ${BASE_IMAGE}
ARG ENVIRONMENT

COPY ${ENVIRONMENT}/watchdog-watchdog /config/watchdog-watchdog/
COPY ${ENVIRONMENT}/configVersion.json /config/watchdog-watchdog/