# This script is meant to only be sourced from within integration.sh and related scripts.

printGreen() {
  IFS=' ' printf "\e[32m%s\e[0m\n" "$*"
}
printYellow() {
  IFS=' ' printf "\e[33m%s\e[0m\n" "$*"
}

trap_ctrl_c() {
  saved_trap=$(trap);
  trap "echo Received SIGINT. Terminating...; exit 1" SIGINT
  "$@"
  eval "$saved_trap"
}

trace() {
  trace_args='$'
  if [[ "${as_single_string:-0}" -eq "1" ]]; then
    trace_args+=$*;
  else
    for trace_arg in "$@"; do
      trace_args+=" $(printf %q "$trace_arg")"
    done
  fi

  printGreen "$trace_args" >&2
  if [[ "${BP:-0}" -eq "1" ||  "${do_step:-0}" -eq "1" ]]; then
    do_step=0
    while : ; do
      trap_ctrl_c read -n1 -r -p "s=step, t=terminate, e=edit command, p=pass (step to the next command without executing the current), r=run a command, c=continue: " key
      echo
      case "$key" in
      s)
        do_step=1
        break
        ;;
      t)
        exit 1
        ;;
      e)
        trap_ctrl_c read -r -e -p ">" -i "$*" command
        echo
        # We call bash to run the command (instead of e.g. calling eval), otherwise the whole script terminates on e.g. unset variables.
        # Now we can just catch those and print an error.
        bash -c -- "$command" || printYellow "Command terminated with exit code $?"
        BP=1 as_single_string=1 trace "$command"
        return
        ;;
      p)
        do_step=1
        return
        ;;
      r)
        trap_ctrl_c read -r -e -p ">" command
        echo
        # See comment in e) case
        bash -c -- "$command" || printYellow "Command terminated with exit code $?"
        BP=1 trace "$@"
        return
        ;;
      c)
        # Do nothing, just break from the read-loop and continue execution
        break
        ;;
      *)
        printYellow "Unrecognized command: $key"
      esac
    done
  fi
  if [[ "${as_single_string:-0}" -eq "1" ]]; then
    eval "$*"
  else
    "$@"
  fi
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

readonly PEPCLI_TIMEOUT=60s
# Executes the given pepcli command. It should always be possible to simply copy a used pepcli command (e.g. during Access Administration) and execute it here.
pepcli() {
  if [ "$LOCAL" = true ]; then
    (
    cd "$DEST_DIR"
    trace timeout -v --kill-after=10s "$PEPCLI_TIMEOUT" "$PEPCLI_COMMAND" --loglevel warning "--client-working-directory" "$CONFIG_DIR/client" "--oauth-token-secret" "$CONFIG_DIR/keyserver/OAuthTokenSecret.json" "$@"
    )
  else
    # Add --interactive to enable piping data via stdin
    # Without the --foreground flag it hangs (on some systems), because of the --interactive flag of docker
    # shellcheck disable=SC2086
    trace timeout --foreground -v --kill-after=10s "$PEPCLI_TIMEOUT" docker exec --interactive -w "/data/client" pepservertest "$PEPCLI_COMMAND" --loglevel warning "--client-working-directory" "$CONFIG_DIR/client" "--oauth-token-secret" "$CONFIG_DIR/keyserver/OAuthTokenSecret.json" "$@"
  fi
}
