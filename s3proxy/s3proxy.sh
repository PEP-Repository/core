#!/usr/bin/env bash

set -o nounset

# Specify a S3PROXY_IMAGE_TAG value (e.g. "sha-ce80406") to (pull and) use that specific tag
# (Some) available tags are listed at https://github.com/gaul/s3proxy/pkgs/container/s3proxy%2Fcontainer
S3PROXY_IMAGE_TAG=
S3PROXY_IMAGE="andrewgaul/s3proxy"
if [ -n "$S3PROXY_IMAGE_TAG" ]; then
  S3PROXY_IMAGE="${S3PROXY_IMAGE}:${S3PROXY_IMAGE_TAG}"
fi

SCRIPTDIR="$( cd "$(dirname "$0")" ; pwd -P )"
SCRIPTPATH="$SCRIPTDIR/$(basename "$0")"
command=${1:-interactive}
pep_network="${2:-}"  # Optional PEP Docker network name for start/interactive commands

docker_exe="docker"
if [ "${OS:-}" = "Windows_NT" ]; then
  docker_exe="${docker_exe}.exe"
fi

# If "command -v" does not find the executable, it exits with a nonzero error code.
# We exit-on-error _after_ this to prevent the script from terminating without output.
docker=$(command -v ${docker_exe})
set -o errexit

countdown() {
  secs=$1
  msg="$2"
  while [ "$secs" -gt 0 ]; do
    sleep 1 & printf "%s %s \r" "$msg" "$secs"
    secs=$(( secs - 1 ))
    wait
  done
  # Clear line and reset cursor to start
  printf "\033[2K\r"
}

# Keep console open long enough to read error messages. See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2619
before_exit() {
  code=$?
  if [ $code -ne 0 ]; then
    countdown 3 "Exiting with code $code in"
  fi
}
trap before_exit EXIT

ensure_docker_available() {
  if [ -z "${docker}" ]; then
    >&2 echo "Could not find '${docker_exe}' in system path; please install"
    exit 1
  fi
}

rel_to_abs() {
  path="$1"
  relative_to="${2:-$SCRIPTDIR}"
  echo "$relative_to/$path"
}

stage() {
  destination="$1"
  s3certs_src="$2"
  skip_crypto="$3"

  >&2 echo "Copying $(basename "$SCRIPTPATH") to $destination"
  mkdir -p "$destination"
  cp "$SCRIPTPATH" "$destination/"

  DATADIR=$(rel_to_abs data "$destination")
  >&2 echo "Creating bucket directories for s3proxy under $DATADIR"
  mkdir -p "${DATADIR}/myBucket"
  mkdir -p "${DATADIR}/myBucket1" # for pepStorageFacilityUnitTests
  mkdir -p "${DATADIR}/myBucket2"

  >&2 echo "Creating configuration directories for s3proxyproxy"
  cp -r "$(rel_to_abs s3proxyproxy_etc_nginx)" "$destination"

  if [ "$skip_crypto" = false ]; then
    >&2 echo "Creating certificates directory for s3proxyproxy"
    s3certs_dest=$(rel_to_abs s3certs "$destination")
    mkdir -p "$s3certs_dest"
    cp "$s3certs_src"/* "$s3certs_dest/"
  fi
}

pull() {
  ensure_docker_available

  >&2 printf "Pulling s3proxy image..."
  "$docker" pull "${S3PROXY_IMAGE}"
  >&2 printf "Pulling nginx (s3proxyproxy) image..."
  "$docker" pull nginx
}

start_containers() {
  ensure_docker_available

  export S3_ACCESS_KEY=MyAccessKey
  export S3_SECRET_KEY=MySecret

  DATADIR=$(rel_to_abs data)
  etc_nginx_dir=$(rel_to_abs s3proxyproxy_etc_nginx)

  # Create "--net" clause for s3proxyproxy's docker command if the pep Docker network was passed,
  # binding that container to that network. This will allow (the integration.sh script
  # to have) other containers (that are bound to the same network) to access the
  # s3proxyproxy container by name.
  network_switch=
  if [ -n "$pep_network" ] ; then
    network_switch="--net $pep_network"
  fi

  # Local paths exposed as a -v volume are prefixed with a slash to prevent Git Bash on Windows
  # from mangling those paths. See https://stackoverflow.com/a/50608818
  >&2 printf "Starting s3proxy container..."
  "$docker" run --rm --detach --name s3proxy -e S3PROXY_IDENTITY=${S3_ACCESS_KEY} -e S3PROXY_CREDENTIAL=${S3_SECRET_KEY} -e LOG_LEVEL=TRACE -p 9001:80 -v /"${DATADIR}":/data "${S3PROXY_IMAGE}"
  >&2 printf "Starting nginx (s3proxyproxy) container..."
  # shellcheck disable=SC2086 # Don't quote possibly empty $network_switch variable so Docker won't interpret them as an empty image spec, causing "invalid reference format" errors
  "$docker" run --rm --detach --name s3proxyproxy $network_switch --add-host=host.docker.internal:host-gateway -p 9000:9000 -v /"${etc_nginx_dir}":/etc/nginx -v /"${SCRIPTDIR}"/s3certs:/s3cert:ro nginx

  # Set ENABLE_S3PROXY_LOG envvar to enable logging
  if [ -n "${ENABLE_S3PROXY_LOG-}" ]; then
    "$docker" logs --follow s3proxy 2> >(sed -u "s/^/[s3proxy]: /" >&2) > >(sed -u "s/^/[s3proxy]: /") &
    "$docker" logs --follow s3proxyproxy 2> >(sed -u "s/^/[s3proxyproxy]: /" >&2) > >(sed -u "s/^/[s3proxyproxy]: /") &
  fi
}

stop_container() {
  container_name="$1"

  ensure_docker_available

  container_status=
  container_running=$("$docker" inspect -f "{{.State.Running}}" "$container_name") && container_status=$? || container_status=$?
  if [ "$container_status" -eq 0 ]; then
    if [ "$container_running" = "true" ]; then
      >&2 printf "Stopping %s..." "$container_name"
      "$docker" stop "$container_name"
    fi
  fi
}

stop_containers() {
  stop_container s3proxyproxy
  stop_container s3proxy
}

run_containers_interactively() {
  start_containers
  printf "Running containers; press <enter> to terminate..."
  # Using "_" to placate shellcheck about unused variable: see https://www.shellcheck.net/wiki/SC2034
  read -r _
  stop_containers
}

case $command in
  "stage") stage "$2" "$3" "${4:-false}";; # stage <destination> <certs-source-dir> [<skip_crypto>]
  "pull") pull;;
  "start") start_containers;;
  "stop") stop_containers;;
  "interactive") run_containers_interactively;;
  *) echo "Unknown command $command"; exit 1;;
esac
