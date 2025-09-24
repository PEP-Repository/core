FROM ubuntu:24.04

ARG STAGING_DIRECTORY=build

COPY ${STAGING_DIRECTORY}/go/src/pep.cs.ru.nl/pep-watchdog/pep-watchdog /app/

EXPOSE 8082

WORKDIR /data
