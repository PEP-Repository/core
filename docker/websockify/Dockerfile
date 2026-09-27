FROM python:3.12-slim

# Install numpy 1.26.4 first (doesn't have strict CPU requirements), then websockify
RUN pip install --no-cache-dir numpy==1.26.4 && \
    pip install --no-cache-dir websockify

COPY run.sh /usr/local/bin/run-websockify.sh

# An environment adds its proxies (and certificate) through the websockify config image, see
# docker/config-dockerfiles/websockify.Dockerfile
ENTRYPOINT ["/usr/local/bin/run-websockify.sh"]
