#!/usr/bin/env sh
# shellcheck disable=SC2039

set -o errexit
set -o nounset

printGreen() {
  IFS=' ' printf "\e[32m%s\e[0m\n" "$*"
}
printYellow() {
  IFS=' ' printf "\e[33m%s\e[0m\n" "$*"
}

trace() {
  printGreen "\$" "$@" "" >&2
  "$@"
}

readonly IMAGE_REPOSITORY="gitlabregistry.pep.cs.ru.nl/pep/core/pep-services"
default_image() {
  commit_sha=$("$1/scripts/gitdir.sh" commit-sha "$1")
  echo "$IMAGE_REPOSITORY:$commit_sha"
}


make_absolute() {
  (cd "$1" && pwd)
}

finish() {
  sig=$?
  
  printGreen "########################################################## Cleanup stage #########################################################"
  printGreen "Output for pepServers:"
  if [ "$LOCAL" = true ]; then
    if [ "${pepServersPid:-x}" != x ]; then
      kill "$pepServersPid" || true
      cat "$DATA_DIR/pepServers.log" || true
    fi
  else
    docker logs pepservertest || true
  fi
  printGreen "End of output of pepServers."
  printYellow "Please ignore any errors below."
  if [ "$USE_DOCKER" = true ]; then
    "$S3PROXY_RUNTIME_DIR/s3proxy.sh" stop
  fi
  if [ "$LOCAL" = false ]; then
    docker stop pepservertest || true
    docker rm pepservertest || true
    docker network rm pep-network || true
  fi
  printYellow " If this script failed, please ignore any errors directly above;"
  printYellow " these are just errors in the \"Cleanup stage\" that do not cause a non-zero exit."
  echo
  if [ "$sig" != 0 ]; then
    printYellow "Error: Exited with code $sig!" >&2
    case "$sig" in
      129) info=SIGHUP    ;;
      130) info=SIGINT    ;;
      131) info=SIGQUIT   ;;
      132) info=SIGILL    ;;
      133) info=SIGTRAP   ;;
      134) info=SIGABRT   ;;
      135) info=SIGBUS    ;;
      136) info=SIGFPE    ;;
      137) info='SIGKILL (out of memory?)' ;;
      138) info=SIGUSR1   ;;
      139) info=SIGSEGV   ;;
      140) info=SIGUSR2   ;;
      141) info=SIGPIPE   ;;
      142) info=SIGALRM   ;;
      143) info=SIGTERM   ;;
      144) info=SIGSTKFLT ;;
      145) info=SIGCHLD   ;;
      146) info=SIGCONT   ;;
      147) info=SIGSTOP   ;;
      148) info=SIGTSTP   ;;
      149) info=SIGTTIN   ;;
      150) info=SIGTTOU   ;;
      151) info=SIGURG    ;;
      152) info=SIGXCPU   ;;
      153) info=SIGXFSZ   ;;
      154) info=SIGVTALRM ;;
      155) info=SIGPROF   ;;
      156) info=SIGWINCH  ;;
      157) info=SIGIO     ;;
      158) info=SIGPWR    ;;
      159) info=SIGSYS    ;;
      *) info= ;;
    esac
    if [ -n "$info" ]; then
      printYellow "$sig = $info" >&2
    fi
  else
    printYellow "Test script terminated without errors. All tests passed."
  fi
}


#execute a command either in docker or locally.
#Arguments:
# workingdir - relative to /data or $DATA_DIR
# command - using the local path. For docker the directory is stripped, and it is assumed to be in /app. Commands which are in PATH (so which are called without a path) will be run as such both in docker and local
# args... - the arguments to pass to command
#
#Environmental variables:
# DOCKER_EXEC_ARGS - can be used to pass additional parameters to "docker exec" for a single invocation,
# e.g. to expose (host environment) variables to the process in the container:
#   DOCKER_EXEC_ARGS="-e HOST_VAR" execute . /app/invoked-with-environment-var.sh
DOCKER_EXEC_ARGS=""
execute() {
  workingdir="$1"
  cmd="$(basename "$2")"
  cmddir="$(dirname "$2")"
  shift 2
  if [ "$LOCAL" = true ]; then
    (
    cd "$DATA_DIR/$workingdir"
    if [ "$cmddir" = "." ]; then
      trace "$cmd" "$@"
    else
      trace "$cmddir/$cmd" "$@"
    fi
    )
  else
    if [ "$cmddir" = "."  ]; then
      # shellcheck disable=SC2086
      trace docker exec $DOCKER_EXEC_ARGS -w "/data/$workingdir" pepservertest "$cmd" "$@"
    else
      # shellcheck disable=SC2086
      trace docker exec $DOCKER_EXEC_ARGS -w "/data/$workingdir" pepservertest "/app/$cmd" "$@"
    fi
  fi
}

# Executes the given pepcli command. It should always be possible to simply copy a used pepcli command (e.g. during Access Administration) and execute it here.
pepcli() {
  if [ "$LOCAL" = true ]; then
    (
    cd "$DEST_DIR"
    trace "$PEPCLI_COMMAND" "--client-working-directory" "$DATA_DIR/client" "--oauth-token-secret" "$DATA_DIR/keyserver/OAuthTokenSecret.json" "$@"
    )
  else
    # shellcheck disable=SC2086
    trace docker exec -w "/data/client" pepservertest "$PEPCLI_COMMAND" "--client-working-directory" "$DATA_DIR/client" "--oauth-token-secret" "$DATA_DIR/keyserver/OAuthTokenSecret.json" "$@"
  fi
}
