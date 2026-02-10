#!/usr/bin/env sh

# Processes constellation JSON files.

# PEP environments are (typically) hosted on multiple machines. Project
# repositories define an environment's involved machines in so-called
# constellation files that specify e.g. machine names, SSH connectivity details,
# and commands needed to restart or update specific services.
# Given such constellation files, this script can update software on the entire
# environment.

set -o errexit
set -o nounset

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

constellation_file="$1"
constellation_command="$2"

# Portable envsubst replacement (But beware: also replaces shell variables and allows command injections)
envsubst() { eval "echo \"$(sed 's/\\/\\\\/g; s/"/\\"/g')\""; }

get_known_host_spec() {
  raw="$1"
  port="$2"
  
  if [ -n "$raw" ]; then
    spec="$raw"
    if [ -n "$port" ]; then
      spec="[$spec]:$port"
    fi
    echo "$spec"
  fi
}

get_known_host_specs() {
  port="$1"
  shift
  
  specs=
  for raw in "$@"; do
    spec=$(get_known_host_spec "$raw" "$port")
    if [ -n "$spec" ]; then
      if [ -n "$specs" ]; then
        specs="$specs,"
      fi
      specs="$specs$spec"
    fi
  done
  echo "$specs"
}

get_host_name() {
  c_entry="$1"
  echo "$c_entry" | jq -r ".name? | select(. != null)"
}

get_host_ip() {
  c_entry="$1"
  echo "$c_entry" | jq -r ".ip? | select(. != null)"
}

get_host_name_or_ip() {
  c_entry="$1"
  
  host=$(get_host_name "$c_entry")
  if [ -z "$host" ]; then
    host=$(get_host_ip "$c_entry")
    if [ -z "$host" ]; then
      >&2 echo Constellation entry has no name and no IP address configured
      exit 1
    fi
  fi
  
  echo "$host"
}

get_host_port() {
  c_entry="$1"
  echo "$c_entry" | jq -r ".ssh.port? | select(. != null)"
}

host_default_user_node="ssh"
host_sync_pull_user_node="sync-pull"
host_sync_push_user_node="sync-push"
host_docker_compose_user_node="docker-compose"
host_user_nodes="$host_default_user_node $host_sync_pull_user_node $host_sync_push_user_node $host_docker_compose_user_node"

get_known_host_line() {
  c_entry="$1"
  
  specs=$(get_known_host_specs "$(get_host_port "$c_entry")" "$(get_host_name "$c_entry")" "$(get_host_ip "$c_entry")")
  if [ -z "$specs" ]; then
    >&2 echo Constellation entry has no name and no IP address configured
    exit 1
  fi
  
  type=$(echo "$c_entry" | jq -r ".ssh.public_key.type")
  base64=$(echo "$c_entry" | jq -r ".ssh.public_key.base64")
  echo "$specs $type $base64"
}

# Converts any "public_key" specifications to entries for the known_hosts file
get_known_hosts_entries() {
  c_json="$1"
  entry_selector="${2:-}"
  
  query='select(.ssh | select(has("public_key")))'
  if [ -n "$entry_selector" ]; then
    query="$entry_selector | $query"
  fi

  echo "$c_json" \
    | jq -c "$query" \
    | while read -r entry
  do
    get_known_host_line "$entry"
  done
}

get_user_json() {
  c_entry="$1"
  custom_parent_node="$2"
  
  user_json=""
  if [ -n "$custom_parent_node" ]; then
    user_json=$(echo "$c_entry" | jq -c ".\"$custom_parent_node\".user | select(. != null)")
  fi
  if [ -z "$user_json" ]; then
    user_json=$(echo "$c_entry" | jq -c ".ssh.user | select(. != null)")
  fi
  
  echo "$user_json"
}

get_abs_path() {
  possibly_relative="$1"
  relative_to="$2"
  
  cd "$relative_to" && realpath "$possibly_relative" && cd - > /dev/null
}

get_user_id_file_path() {
  user_json="$1"
  c_file_dir="$2"
  
  get_abs_path "$(echo "$user_json" | jq -r '.id_file')" "$c_file_dir"
}

get_command_options() {
  port_option_letter="$1"
  c_entry="$2"
  c_file_dir="$3"
  custom_user_parent_node="$4"
  custom_id_file_path="${5:-}"

  # Add the identity file option.
  user_json=$(get_user_json "$c_entry" "$custom_user_parent_node")
  id_file_path="$custom_id_file_path"
  if [ -z "$id_file_path" ]; then
    id_file_path=$(get_user_id_file_path "$user_json" "$c_file_dir")
  fi
  command_options="-i $id_file_path"
  
  # Disable strict host checking if no public key is specified.
  if [ -z "$(echo "$c_entry" | jq '.ssh.public_key? | select(. != null)')" ]; then
    command_options="$command_options -o StrictHostKeyChecking=no"
  fi
  
  # Add port option if a port is specified.
  port=$(get_host_port "$c_entry")
  if [ -n "$port" ]; then
    command_options="$command_options -$port_option_letter $port"
  fi
  
  echo "$command_options"
}

get_user_at_host() {
  c_entry="$1"
  custom_user_parent_node="$2"
  
  user_json=$(get_user_json "$c_entry" "$custom_user_parent_node")
  user=$(echo "$user_json" | jq -r ".name")
  host=$(get_host_name_or_ip "$c_entry")
  echo "$user@$host"
}

run_cmd() {
  echo "$1"
  eval "$1"
}

get_user_entries() {
  c_json="$1"
  entry_selector="$2"
  custom_parent_node="${3:-}"
  
  hosts=$(echo "$c_json" | tr '\n' ' ')
  if [ -n "$entry_selector" ]; then
    hosts=$(echo "$c_json" | jq -c "$entry_selector")
  fi
  echo "$hosts" | while read -r host; do
    user_json=$(get_user_json "$host" "$custom_parent_node")
    if [ -n "$user_json" ] && [ "$user_json" != "null" ]; then
      echo "$user_json"
    fi
  done | sort | uniq
}

prepare_ssh_connectivity() {
  c_json="$1"
  c_file_dir="$2"
  entry_selector="${3:-}"
  
  known_hosts=$(get_known_hosts_entries "$c_json" "$entry_selector")
  
  if [ -n "$known_hosts" ]; then
    echo "Adding constellation to known hosts..."
    run_cmd "mkdir -p ~/.ssh"
    run_cmd "touch ~/.ssh/known_hosts"
    echo "$known_hosts" | while read -r entry; do
      run_cmd "echo \"$entry\" >> ~/.ssh/known_hosts"
    done
  fi
  
  echo "Making SSH ID file(s) usable..."
  echo "$host_user_nodes" | tr ' ' '\n' | while read -r user_node; do
    get_user_entries "$c_json" "$entry_selector" "$user_node" | while read -r user_json; do
      get_user_id_file_path "$user_json" "$c_file_dir"
    done
  done | sort | uniq | while read -r abs_path; do
    run_cmd "chmod 400 \"$abs_path\""
  done
}

get_ssh_command() {
  c_entry="$1"
  c_file_dir="$2"
  remote_cmd_selector="${3:-}"
  custom_user_parent_node="${4:-}"
  custom_id_file_path="${5:-}"
  
  # Specify -n to prevent ssh from reading stdin, making the command invokable from a pipe-to-while-read loop
  # See https://stackoverflow.com/a/13800476
  ssh_command="ssh -n $(get_command_options p "$c_entry" "$c_file_dir" "$custom_user_parent_node" "$custom_id_file_path") $(get_user_at_host "$c_entry" "$custom_user_parent_node")"
  
  if [ -n "$remote_cmd_selector" ]; then
    remote_cmd=$(echo "$c_entry" | jq -r "$remote_cmd_selector | select(. != null)")
    if [ -n "$remote_cmd" ]; then
      ssh_command="$ssh_command $remote_cmd"
    fi
  fi
  
  echo "$ssh_command"
}

# Runs host-wide SSH triggers to pull updates for all services on that host
get_update_host_commands() {
  c_json="$1"
  c_file_dir="${2:-.}"

  echo "$c_json" \
    | jq -c '.[] | select(.update | type=="string" or type=="null") | select(has("docker-compose") | not)' \
    | while read -r entry
  do
    get_ssh_command "$entry" "$c_file_dir" ".update"
  done
}

get_service_cmd() {
  c_entry="$1"
  update_or_restart="$2"
  service_name="$3"
  c_file_dir="${4:-.}"
  
  if [ -n "$(echo "$c_entry" \
    | jq "select(.$update_or_restart? | type==\"object\")" \
    | jq "select(.$update_or_restart | has(\"$service_name\"))" \
    )" ]
  then
    get_ssh_command "$c_entry" "$c_file_dir" ".$update_or_restart.\"$service_name\""
  fi
}

get_restart_service_command() {
  c_json="$1"
  c_file_dir="$2"
  service_name="$3"
  
  echo "$c_json" \
    | jq -c '.[] | select(has("docker-compose") | not)' \
    | while read -r entry
  do
    get_service_cmd "$entry" "update" "$service_name" "$c_file_dir"
    get_service_cmd "$entry" "restart" "$service_name" "$c_file_dir"
  done
}

get_service_restart_commands() {
  c_json="$1"
  c_file_dir="${2:-.}"
  
  get_restart_service_command "$c_json" "$c_file_dir" "transcryptor"
  get_restart_service_command "$c_json" "$c_file_dir" "keyserver"
  get_restart_service_command "$c_json" "$c_file_dir" "storagefacility"
  get_restart_service_command "$c_json" "$c_file_dir" "accessmanager"
  get_restart_service_command "$c_json" "$c_file_dir" "registrationserver"
  get_restart_service_command "$c_json" "$c_file_dir" "authserver"
  get_restart_service_command "$c_json" "$c_file_dir" "authserver-apache"
  get_restart_service_command "$c_json" "$c_file_dir" "watchdog"
}

update() {
  c_json="$1"
  c_file_dir="${2:-.}"
  
  update_hosts=$(get_update_host_commands "$c_json" "$c_file_dir")
  restart_services=$(get_service_restart_commands "$c_json" "$c_file_dir")
  
  prepare_ssh_connectivity "$c_json" "$c_file_dir" '.[] | select(has("docker-compose") | not)'
  
  if [ -n "$update_hosts" ]; then
    echo "Updating $(echo "$update_hosts" | wc -l) host(s)..."
    echo "$update_hosts" | while read -r cmd; do
      run_cmd "$cmd"
    done
    if [ -n "$restart_services" ]; then
      echo "Giving host(s) time to finish updating themselves..."
      run_cmd "sleep 60"
    fi
  fi
  
  if [ -n "$restart_services" ]; then
    echo "Restarting $(echo "$restart_services" | wc -l) service(s)..."
    echo "$restart_services" | {
      pre_service_cmd=
      while read -r cmd; do
        if [ -n "$pre_service_cmd" ]; then
          run_cmd "$pre_service_cmd"
        fi
        run_cmd "$cmd"
        pre_service_cmd="sleep 10"
      done
    }
  fi
}

get_sftp_command() {
  c_entry="$1"
  c_file_dir="$2"
  local_path="$3"
  remote_path="$4"
  custom_user_parent_node="${5:-}"

  echo "echo \"put -R $local_path\" | sftp -v -b - $(get_command_options P "$c_entry" "$c_file_dir" "$custom_user_parent_node") $(get_user_at_host "$c_entry" "$custom_user_parent_node"):$remote_path"
}

publish() {
  c_entry="$1"
  c_file_dir="$2"
  local_path="$3"
  remote_path="${4:-.}"
  
  prepare_ssh_connectivity "$c_entry" "$c_file_dir"
  cmd=$(get_sftp_command "$c_entry" "$c_file_dir" "$local_path" "$remote_path")
  run_cmd "$cmd"
  echo Published "$local_path" to "$(get_host_name_or_ip "$c_entry")"/"$remote_path"
}

get_file_dir() {
  file="$1"
  dirname "$(realpath "$file")"
}

get_pull_constellation_entry() {
  pull_c_json="$1"
  echo "$pull_c_json" | jq -c '.[] | select(."sync-pull")'
}

update_data_sync() {
  pull_c_json="$1"
  pull_c_file_dir="$2"
  push_c_file="$3"
  
  pull_c_entry=$(get_pull_constellation_entry "$pull_c_json")
  push_c_file_contents=$(envsubst < "$push_c_file")
  push_c_json=$(echo "$push_c_file_contents" | jq -c '.[] | select(has("sync-push"))')
  if [ -z "$pull_c_entry" ]; then
    echo "No pull configuration found in constellation"
  elif [ -z "$push_c_json" ]; then
    echo "No push configuration found in $push_c_file"
  else
    # Clear pull host's "data-sync" directory
    prepare_ssh_connectivity "$pull_c_entry" "$pull_c_file_dir"
    run_cmd "$(get_ssh_command "$pull_c_entry" "$pull_c_file_dir" "" "sync-pull") 'rm -R -f data-sync && mkdir data-sync'"

    # Prepare local "data-sync" directory
    rm -R -f build/data-sync
    mkdir --parents build/data-sync
    cp "$SCRIPTPATH/data-sync/pull.py" build/data-sync/
    get_known_hosts_entries "$push_c_json" > build/data-sync/push.known_hosts
    echo "#!/usr/bin/env sh" > build/data-sync/push.sh
    echo "set -o errexit"   >> build/data-sync/push.sh
    chmod 755 build/data-sync/push.sh
    
    push_c_file_dir="$(get_file_dir "$push_c_file")"
    echo "$push_c_json" | while read -r push_c_entry; do
      user_json=$(get_user_json "$push_c_entry" "sync-push")
      id_file_src=$(get_user_id_file_path "$user_json" "$push_c_file_dir")
      id_file_dest="data-sync/$(get_user_at_host "$push_c_entry" "sync-push")"
      cp "$id_file_src" "build/$id_file_dest"
      chmod 400 "build/$id_file_dest"

      script_src=$(echo "$push_c_entry" | jq -r '."sync-push".script | select(. != null)')
      invoke_script=""
      if [ -n "$script_src" ]; then
        script_src=$(get_abs_path "$script_src" "$push_c_file_dir")
        # Copy script to prod host
        invoke_script="\"./data-sync/$(basename "$script_src")\""
        # TODO: only prepare affected ID file and known host entry
        prepare_ssh_connectivity "$push_c_entry" "$push_c_file_dir"
        echo "Copying data sync file(s) to push host..."
        run_cmd "$(get_sftp_command "$push_c_entry" "$push_c_file_dir" "$script_src" "data-sync/" "sync-push")"
      fi

      # Add prod host to hosts-to-invoke when synchronizing
      # shellcheck disable=SC2088 # Don't warn about "Tilde does not expand in quotes": we want the home directory on SSH host, not the local one
      echo "$(get_ssh_command "$push_c_entry" "$push_c_file_dir" "" "sync-push" "~/$id_file_dest") -o \"UserKnownHostsFile ~/data-sync/push.known_hosts\" $invoke_script \"\$@\"" >> build/data-sync/push.sh
    done
    
    echo "Copying data sync file(s) to pull host..."
    run_cmd "$(get_sftp_command "$pull_c_entry" "$pull_c_file_dir" build/data-sync . "sync-pull")"
  fi
}

sync_data() {
  pull_c_json="$1"
  pull_c_file_dir="$2"
  
  pull_c_entry=$(get_pull_constellation_entry "$pull_c_json")
  if [ -z "$pull_c_entry" ]; then
    echo "No pull configuration found in constellation"
  else
    shift
    shift
    prepare_ssh_connectivity "$pull_c_entry" "$pull_c_file_dir"
    remote_cmd="./data-sync/pull.py $*"
    run_cmd "$(get_ssh_command "$pull_c_entry" "$pull_c_file_dir" "" "sync-pull") '$remote_cmd'"
  fi
}

get_docker_compose_commands() {
  c_json="$1"
  c_file_dir="${2:-.}"
  compose_image="$3"
  action="${4:-redeploy}"  # pull, restart, stop, or redeploy
  override_profiles="${5:-}"  # Optional profiles override
  
  # Use jq selector to filter docker-compose hosts directly
  echo "$c_json" \
    | jq -c '.[] | select(has("docker-compose"))' \
    | while read -r entry
  do
    # Use the docker-compose specific configuration or fall back to default ssh user
    ssh_cmd=$(get_ssh_command "$entry" "$c_file_dir" "" "docker-compose")
    
    # Use override profiles if provided, otherwise extract from constellation config
    if [ -n "$override_profiles" ]; then
      profiles="$override_profiles"
    else
      profiles=$(echo "$entry" | jq -r '.["docker-compose"].profiles // empty | if type == "array" then join(",") else . end')
    fi
    
    # Build the remote command with profiles
    if [ -n "$profiles" ]; then
      remote_cmd="$action $compose_image $profiles"
    else
      remote_cmd="$action $compose_image"
    fi
    
    echo "$ssh_cmd '$remote_cmd'"
  done
}

docker_compose() {
  c_json="$1"
  c_file_dir="${2:-.}"
  compose_image="$3"
  action="${4:-redeploy}"  # pull, restart, stop, or redeploy
  override_profiles="${5:-}"  # Optional profiles override
  
  docker_compose_commands=$(get_docker_compose_commands "$c_json" "$c_file_dir" "$compose_image" "$action" "$override_profiles")
  
  prepare_ssh_connectivity "$c_json" "$c_file_dir" '.[] | select(has("docker-compose"))'
  
  if [ -n "$docker_compose_commands" ]; then
    case "$action" in
      "pull")
        action_verb="Pulling images"
        ;;
      "restart")
        action_verb="Restarting services"
        ;;
      "stop")
        action_verb="Stopping services"
        ;;
      "redeploy")
        action_verb="Redeploying (pull, stop, and start)"
        ;;
      *)
        action_verb="Executing $action"
        ;;
    esac
    
    if [ -n "$override_profiles" ]; then
      echo "$action_verb with docker-compose using profiles [$override_profiles] on $(echo "$docker_compose_commands" | wc -l) host(s)..."
    else
      echo "$action_verb with docker-compose on $(echo "$docker_compose_commands" | wc -l) host(s)..."
    fi
    
    echo "$docker_compose_commands" | while read -r cmd; do
      run_cmd "$cmd"
    done
  else
    echo "No docker-compose configuration found in constellation"
  fi
}

# We're using envsubst to expand environment variables in the constellation file
constellation_json=$(envsubst < "$constellation_file")
constellation_file_dir="$(get_file_dir "$constellation_file")"

case $constellation_command in
  update)
    update "$constellation_json" "$constellation_file_dir"
    ;;
  docker-compose)
    compose_image="${3:-}"
    action="${4:-redeploy}"
    override_profiles="${5:-}"
    if [ -z "$compose_image" ]; then
      echo "Error: compose image must be provided as third argument"
      echo "Usage: $0 constellation.json docker-compose <compose_image> [<action>] [<profiles>]"
      exit 1
    fi
    docker_compose "$constellation_json" "$constellation_file_dir" "$compose_image" "$action" "$override_profiles"
    ;;
  update-data-sync)
    push_constellation_file="$3"
    if [ -f "$push_constellation_file" ]; then
      update_data_sync "$constellation_json" "$constellation_file_dir" "$push_constellation_file"
    else
      echo "Synchronization source constellation file not found at $push_constellation_file"
    fi
    ;;
  publish)
    local_path="$3"
    remote_path="${4:-.}"
    publish "$constellation_json" "$constellation_file_dir" "$local_path" "$remote_path"
    ;;
  sync-data)
    # Drop the constellation_file and constellation_command arguments so we can...
    shift
    shift
    # ... forward remaining arguments to the script invoked on the pull host
    sync_data "$constellation_json" "$constellation_file_dir" "$@"
    ;;
  *)
    >&2 echo Unsupported constellation command: "$constellation_command"
    exit 1
esac
