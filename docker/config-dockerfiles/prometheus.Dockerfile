FROM prom/prometheus:v3.2.1
ARG ENVIRONMENT=master

COPY ${ENVIRONMENT}/prometheus/ /etc/prometheus/