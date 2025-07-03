ARG ENVIRONMENT=master
ARG BASE_IMAGE
FROM ${BASE_IMAGE}
ARG ENVIRONMENT

COPY ${ENVIRONMENT}/pep-monitoring /config/
COPY ${ENVIRONMENT}/rootCA.cert /config/watchdog/
COPY ${ENVIRONMENT}/configVersion.json /config/watchdog/
