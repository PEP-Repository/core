ARG ENVIRONMENT=main
# check=skip=InvalidDefaultArgInFrom
ARG BASE_IMAGE
FROM ${BASE_IMAGE}
ARG ENVIRONMENT

COPY ${ENVIRONMENT}/docker-compose /compose/