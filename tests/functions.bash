# This script is meant to only be sourced from within integration.sh and related scripts.

printGreen() {
  IFS=' ' printf "\e[32m%s\e[0m\n" "$*"
}
printYellow() {
  IFS=' ' printf "\e[33m%s\e[0m\n" "$*"
}

trace() {
  trace_args='$'
  for trace_arg in "$@"; do
    trace_args+=" $(printf %q "$trace_arg")"
  done

  printGreen "$trace_args" >&2
  "$@"
}

fail() {
  >&2 printYellow echo "$@"
  exit 1
}

readonly IMAGE_REPOSITORY="gitlabregistry.pep.cs.ru.nl/pep/core/pep-services"
default_image() {
  commit_sha=$("$1/scripts/gitdir.sh" commit-sha "$1")
  echo "$IMAGE_REPOSITORY:$commit_sha"
}


make_absolute() {
  (cd "$1" && pwd)
}

contains() {
  string="$1"
  substring="$2"
  # `&& true` prevents quitting for nonzero exit code
  [ "${string#*"$substring"}" != "$string" ] && true
}

should_run_test() {
  test="$1"
  if ([ -z "$TESTS_TO_RUN" ] || contains " $TESTS_TO_RUN " " $test ") && ! contains " $TESTS_TO_SKIP " " $test "; then
    echo
    printGreen "==== Running tests: $test ===="
  else
    echo
    printGreen "(Skipping tests: $test)"
    return 1
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
