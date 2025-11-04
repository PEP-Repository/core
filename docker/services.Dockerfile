ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ARG STAGING_DIRECTORY=build

# Copy run and configuration scripts, and PEP executables
COPY \
    ${STAGING_DIRECTORY}/cpp/pep/accessmanager/pepAccessManager \
    ${STAGING_DIRECTORY}/cpp/pep/accessmanager/pepAccessManagerUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/apps/pepClientTest \
    ${STAGING_DIRECTORY}/cpp/pep/apps/pepDumpShadowAdministration \
    ${STAGING_DIRECTORY}/cpp/pep/apps/pepEnrollment \
    ${STAGING_DIRECTORY}/cpp/pep/apps/pepGenerateSystemKeys docker/init_keys.sh docker/config_servers.sh \
    ${STAGING_DIRECTORY}/cpp/pep/apps/pepToken \
    ${STAGING_DIRECTORY}/cpp/pep/archiving/pepArchivingUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/async/pepAsyncUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/auth/pepAuthUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/authserver/pepAuthserver \
    ${STAGING_DIRECTORY}/cpp/pep/benchmark/pepbenchmark \
    ${STAGING_DIRECTORY}/cpp/pep/cli/pepcli \
    ${STAGING_DIRECTORY}/cpp/pep/logon/pepLogon \
    ${STAGING_DIRECTORY}/cpp/pep/content/pepContentUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/crypto/pepCryptoUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/elgamal/pepElgamalUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/keyserver/pepKeyServer \
    ${STAGING_DIRECTORY}/cpp/pep/keyserver/pepKeyServerUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/morphing/pepMorphingUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/networking/pepNetworkingUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/rsk/pepRskUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/rsk-pep/pepRskPepUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/registrationserver/pepRegistrationServer \
    ${STAGING_DIRECTORY}/cpp/pep/serialization/pepSerializationUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/storagefacility/pepStorageFacility \
    ${STAGING_DIRECTORY}/cpp/pep/storagefacility/pepStorageFacilityUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/structure/pepStructureUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/structuredoutput/pepStructuredOutputUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/ticketing/pepTicketingUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/transcryptor/pepTranscryptor \
    ${STAGING_DIRECTORY}/cpp/pep/utils/pepUtilsUnitTests \
    ${STAGING_DIRECTORY}/cpp/pep/versioning/pepVersioningUnitTests \
    ${STAGING_DIRECTORY}/go/src/pep.cs.ru.nl/pep-watchdog/pep-watchdog \
    docker/run.sh \
    docker/run_keyserver.sh \
    pki/ca_ext.cnf \
    pki/pki.sh \
        /app/

COPY web/templates /app/templates


EXPOSE 16501 16511 16512 16516 16518 16519 8080


# Autocompletion
COPY ./${STAGING_DIRECTORY}/autocomplete/ /tmp/autocomplete_pep
RUN /tmp/autocomplete_pep/install_autocomplete_pep.sh

# ==== bash ====
RUN echo 'PATH="/app/:$PATH"' >>/root/.bashrc

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

# ==== zsh ====
RUN echo 'PATH="/app/:$PATH"' >>/etc/zsh/zshrc

COPY ./docker/zsh.zshrc /root/.zshrc


WORKDIR /data
ENV PEP_LOGON_LIMITED=1

CMD bash /app/run.sh
