ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ARG STAGING_DIRECTORY=build

# Copy PEP executables
COPY ${STAGING_DIRECTORY}/cpp/pep/cli/pepcli \
     ${STAGING_DIRECTORY}/cpp/pep/logon/pepLogon \
     ${STAGING_DIRECTORY}/cpp/pep/apps/pepClientTest \
     ${STAGING_DIRECTORY}/cpp/pep/apps/pepEnrollment \
     ${STAGING_DIRECTORY}/cpp/pep/apps/pepToken \
     ${STAGING_DIRECTORY}/cpp/pep/apps/pepGenerateSystemKeys \
     ${STAGING_DIRECTORY}/cpp/pep/apps/pepDumpShadowAdministration \
     ${STAGING_DIRECTORY}/cpp/pep/pullcastor/pepPullCastor \
     /app/

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
