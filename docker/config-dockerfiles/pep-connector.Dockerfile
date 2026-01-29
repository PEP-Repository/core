ARG ENVIRONMENT=main
ARG BASE_IMAGE
FROM ${BASE_IMAGE}
ARG ENVIRONMENT
ARG PROJECT_DIR

# Set environment variable to find config
ENV PEP_CONFIG_DIR="/config"

# Combine config files from project and environment directories
COPY ${PROJECT_DIR}/client /config/
COPY ${ENVIRONMENT}/client ${ENVIRONMENT}/rootCA.cert ${ENVIRONMENT}/configVersion.json ${ENVIRONMENT}/ShadowAdministration.pub /config/
