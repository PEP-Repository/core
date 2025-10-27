ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ARG STAGING_DIRECTORY=build

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends python3-pip && \
    apt-get clean && rm -rf /var/cache/* /var/lib/{apt,dpkg,cache,log}/* /tmp/* /var/tmp/*

# Install the wheel with a bind mount
RUN --mount=type=bind,source=${STAGING_DIRECTORY}/python/pep/pep_connector/,target=/tmp/pep_connector \
    pip install --break-system-packages --no-cache-dir /tmp/pep_connector/*.whl

