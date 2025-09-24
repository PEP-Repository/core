FROM docker
ARG ENVIRONMENT

COPY ${ENVIRONMENT}/docker-compose/ /compose