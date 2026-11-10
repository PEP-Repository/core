#!/usr/bin/env bash
# Usage: mem-gate.sh <command> [args...]
#
# Runs a memory-hungry <command> (e.g. a compiler or clang-tidy invocation) such that running out of memory is avoided,
# and survived if it happens anyway:
# - Waits until there seems to be enough available memory before starting <command>. Since a compiler's memory usage
#   grows during its first seconds, commands started recently are counted as already using their expected memory.
# - Makes <command> the preferred victim of the OOM killer, instead of e.g. Ninja or the CI job's shell.
# - Reruns <command> if it was killed by the OOM killer, requiring more available memory and one rerun at a time.
#
# Settings (environment variables):
#   MEM_GATE_COMMAND_MIB       Expected peak memory usage of <command> (default 1024)
#   MEM_GATE_RESERVE_MIB       Memory to keep available for other processes (default 1024)
#   MEM_GATE_RAMP_SECONDS      Time during which a started command is assumed not to use its memory yet (default 5)
#   MEM_GATE_MAX_WAIT_SECONDS  Start <command> anyway after waiting this long (default 600)
#   MEM_GATE_MAX_ATTEMPTS      Maximum number of times to run <command> (default 3)
#   MEM_GATE_STATE_DIR         Directory to coordinate commands in; can be shared between containers to also coordinate
#                              commands of different CI jobs (default ${TMPDIR:-/tmp}/mem-gate)
#   MEM_GATE_LOG_FD            Open file descriptor to write messages to, instead of stderr. Use this when running via
#                              ccache's prefix_command, as ccache would otherwise store our messages along with the
#                              compiler's output and replay them on every cache hit. Messages written to an inherited
#                              descriptor also bypass the output buffering of Ninja

command_mib=${MEM_GATE_COMMAND_MIB:-1024}
reserve_mib=${MEM_GATE_RESERVE_MIB:-1024}
ramp_seconds=${MEM_GATE_RAMP_SECONDS:-5}
max_wait_seconds=${MEM_GATE_MAX_WAIT_SECONDS:-600}
max_attempts=${MEM_GATE_MAX_ATTEMPTS:-3}
state_dir=${MEM_GATE_STATE_DIR:-${TMPDIR:-/tmp}/mem-gate}
command_name=${1##*/}

# Our own cgroup (v2), which is the cgroup root inside a container
cgroup_dir=/sys/fs/cgroup$(sed -n 's/^0:://p' /proc/self/cgroup 2>/dev/null)

# Prints available memory in MiB: the minimum of what the system and our cgroup's limit (if any) allow
available_mib() {
  local available limit current inactive
  available=$(awk '$1 == "MemAvailable:" { print int($2 / 1024) }' /proc/meminfo)
  if [[ -r $cgroup_dir/memory.max ]] && limit=$(<"$cgroup_dir/memory.max") && [[ $limit != max ]]; then
    current=$(<"$cgroup_dir/memory.current")
    # Inactive page cache can be reclaimed
    inactive=$(awk '$1 == "inactive_file" { print $2 }' "$cgroup_dir/memory.stat")
    (( limit = (limit - current + ${inactive:-0}) / 1048576 ))
    if (( limit < available )); then
      available=$limit
    fi
  fi
  echo "$available"
}

log() {
  local message="mem-gate $$: $*"
  # Fall back to stderr if the descriptor is not open (anymore)
  if [[ -z ${MEM_GATE_LOG_FD-} ]] || ! { echo "$message" >&"$MEM_GATE_LOG_FD"; } 2>/dev/null; then
    echo "$message" >&2
  fi
}

# Prints the number of processes in our cgroup killed by the OOM killer, or nothing if unknown
oom_kills() {
  awk '$1 == "oom_kill" { print $2 }' "$cgroup_dir/memory.events" 2>/dev/null
}

# Waits until $1 MiB seems available (on top of the reserve), then registers the start of a command needing it.
# The "starts" file lists "<start time in µs> <MiB>" of recently started commands.
wait_for_memory() {
  local need=$1 wait_start=$EPOCHSECONDS waiting=false now_us start_us start_mib recent_mib fits
  local -a recent
  while true; do
    flock "$lock_fd"
    now_us=${EPOCHREALTIME/[.,]/}
    recent=()
    recent_mib=0
    if [[ -f $state_dir/starts ]]; then
      while read -r start_us start_mib; do
        if [[ -n $start_mib ]] && (( now_us - start_us < ramp_seconds * 1000000 )); then
          recent+=("$start_us $start_mib")
          (( recent_mib += start_mib ))
        fi
      done <"$state_dir/starts"
    fi

    fits=false
    if (( $(available_mib) - recent_mib >= need + reserve_mib )); then
      fits=true
    elif (( EPOCHSECONDS - wait_start >= max_wait_seconds )); then
      log "Still not ${need} MiB of available memory after ${max_wait_seconds}s, giving up waiting"
      fits=true
    fi
    if $fits; then
      recent+=("$now_us $need")
    fi
    printf '%s\n' "${recent[@]}" >"$state_dir/starts"
    flock --unlock "$lock_fd"

    if $fits; then
      if $waiting; then
        log "Starting $command_name after waiting $(( EPOCHSECONDS - wait_start ))s"
      fi
      return
    fi
    if ! $waiting; then
      log "Waiting for ${need} MiB of available memory to start $command_name"
      waiting=true
    fi
    local sleep_time=0.$(( RANDOM % 5 + 5 ))s
    sleep "$sleep_time"
  done
}

# If we cannot coordinate, don't gate (but still rerun when OOM-killed)
lock_fd=
if mkdir -p "$state_dir" 2>/dev/null; then
  # `{ ... ; } 2>...`: group for stderr redirection, but don't start a subshell
  # `exec ...>...`: apply redirections to current script
  # `{lock_fd}`: store new file descriptor into lock_fd
  # So basically: try to grab a file descriptor to the lock file
  { exec {lock_fd}>>"$state_dir/lock"; } 2>/dev/null || lock_fd=
fi

need_mib=$command_mib
for (( attempt = 1; ; attempt++ )); do
  if [[ -n $lock_fd ]]; then
    wait_for_memory "$need_mib"
  fi

  oom_kills_before=$(oom_kills)
  # Raise the OOM score for <command> only (via a subshell, not for this script), which does not require privileges.
  # Bash's own "Killed" notice is discarded, while <command> gets the original stderr back via fd 3
  {
    (
      { echo 1000 >/proc/self/oom_score_adj; } 2>/dev/null
      exec "$@" 2>&3 3>&-
    )
  } 3>&2 2>/dev/null
  status=$?

  # 137: killed by SIGKILL. If our cgroup's OOM kill count is known, it should have increased
  #  (otherwise someone explicitly killed it)
  if (( status != 137 || attempt >= max_attempts )) ||
     [[ -n $oom_kills_before && $(oom_kills) == "$oom_kills_before" ]]; then
    exit "$status"
  fi

  log "$command_name was killed by the OOM killer, rerunning it (attempt $(( attempt + 1 ))/$max_attempts)"
  (( need_mib *= 2 ))
  # Rerun one command at a time, so that heavy commands do not keep killing each other
  if [[ -z ${retry_lock_fd-} ]] && { exec {retry_lock_fd}>>"$state_dir/retry.lock"; } 2>/dev/null; then
    # retry_lock_fd will be unlocked when we exit
    flock "$retry_lock_fd"
  fi
done
