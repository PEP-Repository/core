ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

# Install jq for JSON parsing in fake peplogon script
RUN apt-get update && \
    apt-get upgrade -y --autoremove --purge && \
    apt-get install -y --no-install-recommends jq && \
    apt-get clean && rm -rf /var/cache/* /var/lib/{apt,dpkg,cache,log}/* /tmp/* /var/tmp/*

ARG STAGING_DIRECTORY=build

COPY ${STAGING_DIRECTORY}/cpp/pep/servers/pepServers \
     ${STAGING_DIRECTORY}/cpp/pep/servers/configVersion.json /app/
COPY ${STAGING_DIRECTORY}/cpp/pep/servers/authserver /app/authserver/
RUN echo -n "be67c8dd6a141ded148861ac11aef64d68640876601a40dca01816004e4c5161" > /app/authserver/spoofKey
COPY ${STAGING_DIRECTORY}/cpp/pep/servers/keyserver /app/keyserver/
COPY ${STAGING_DIRECTORY}/cpp/pep/servers/registrationserver /app/registrationserver/
COPY ${STAGING_DIRECTORY}/cpp/pep/servers/storagefacility /app/storagefacility/
COPY ${STAGING_DIRECTORY}/cpp/pep/servers/transcryptor /app/transcryptor/
COPY ${STAGING_DIRECTORY}/cpp/pep/servers/accessmanager /app/accessmanager/
RUN echo '{\
  "participant_identifier_formats": [{"generable": {"prefix": "OID", "digits": 10}}],\
  "user_pseudonym_format": {"prefix": "BLPS", "minLength": 1, "length": 16},\
  "short_pseudonyms": [{"column": "ShortPseudonym.History", "prefix": "RIDHI", "length": 5}]\
}' > /app/accessmanager/GlobalConfiguration.json

COPY ${STAGING_DIRECTORY}/cpp/pep/cli/pepcli /app/
COPY ${STAGING_DIRECTORY}/cpp/pep/cli/ClientConfig.json \
     ${STAGING_DIRECTORY}/cpp/pep/cli/rootCA.cert \
     ${STAGING_DIRECTORY}/cpp/pep/cli/configVersion.json \
     ${STAGING_DIRECTORY}/cpp/pep/cli/ShadowAdministration.pub \
     /config/

# Autocompletion
COPY ./${STAGING_DIRECTORY}/autocomplete/ /tmp/autocomplete_pep
RUN /tmp/autocomplete_pep/install_autocomplete_pep.sh

# ==== bash ====
RUN echo 'PATH="/app:/config/:$PATH"' >>/root/.bashrc

# This is commented-out by default in /etc/bash.bashrc on Ubuntu, so add it
# Explicitly call bash for echo -e
RUN bash -c "echo -e '\n\
if ! shopt -oq posix; then\n\
  if [ -f /usr/share/bash-completion/bash_completion ]; then\n\
    . /usr/share/bash-completion/bash_completion\n\
  elif [ -f /etc/bash_completion ]; then\n\
    . /etc/bash_completion\n\
  fi\n\
fi\n\
' >>/etc/bash.bashrc"

WORKDIR /data

# Copy pepLogon script
COPY docker/fake-pepLogon /app/pepLogon
RUN chmod +x /app/pepLogon

# Copy workshop setup scripts
COPY docker/entrypoint-workshop.sh /
RUN chmod +x /entrypoint-workshop.sh

ENV PEP_LOGON_LIMITED=1
ENV PEP_CONFIG_DIR="/config"

ENV DEBIAN_FRONTEND=''

# Use workshop entrypoint for automatic setup and server execution
ENTRYPOINT ["/entrypoint-workshop.sh"]
