#!/usr/bin/env sh

# Processes config dockerfiles for PEP project repositories.

# (The Linux version of) PEP is distributed as Docker images.
# 1. The PEP source repository produces images containing the PEP binaries.
# 2. PEP project repositories use the binary images as a base and add
#   configuration to run PEP for a specific environment.
# This script supports the second step:
# - for subdirectories of the project repository's "config" directory,
# - if the name of the subdirectory matches one of the files in the PEP source
#   repository's "docker/config-dockerfiles",
# - then this script can build a Docker image based on the source repo's
#   (Docker)file and the project repo's (config) directory.
# When building such a "config dockerfile", if the config image should be based
# on a PEP (source repo) binary Docker image, the script locates that binary
# image in the source repo's registry.

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

# Portable envsubst replacement (But beware: also replaces shell variables and allows command injections)
envsubst() { eval "echo \"$(sed 's/\\/\\\\/g; s/"/\\"/g')\""; }

git_dir=""
environment=""
images_tag=""
foss_dir=""
api_key=""
with_rsyslog=""
rsyslog_dir=""

usage() {
  exit_code=${1:-1}
  cat << EOF
Usage: $0 [OPTIONS]

Options:
  -g, --git-dir DIR        Git directory
  -e, --env ENV            Environment name
  -t, --tag TAG            Pipeline tag for images
  -f, --foss-dir DIR       FOSS directory
  -k, --api-key KEY        Registry API token
  -r, --with-rsyslog BOOL  Force enable/disable rsyslog (true|false)
  -d, --rsyslog-dir DIR    Rsyslog configuration directory
  -h, --help               Show this help message
EOF
  exit "$exit_code"
}

while [ $# -gt 0 ]; do
  case "$1" in
    -g|--git-dir)       shift; git_dir="$1" ;;
    -e|--env)           shift; environment="$1" ;;
    -t|--tag)           shift; images_tag="$1" ;;
    -f|--foss-dir)      shift; foss_dir="$1" ;;
    -k|--api-key)       shift; api_key="$1" ;;
    -r|--with-rsyslog)  shift; with_rsyslog="$1" ;;
    -d|--rsyslog-dir)   shift; rsyslog_dir="$1" ;;
    -h|--help)          usage 0 ;;
    *) >&2 echo "Unknown option: $1"; usage 1 ;;
  esac
  shift
done

# Verify required parameters for all commands
if [ -z "$git_dir" ] || [ -z "$environment" ] || [ -z "$images_tag" ] || [ -z "$foss_dir" ] || [ -z "$api_key" ]; then
  >&2 echo "Error: Missing required parameters."
  usage 1
fi

# Verify rsyslog parameters
if [ "$with_rsyslog" = "true" ] && [ -z "$rsyslog_dir" ]; then
  >&2 echo "Error: --with-rsyslog=true but no rsyslog directory (--rsyslog-dir) specified."
  usage 1
fi

# shellcheck source=ci_cd/project-common.sh
. "$SCRIPTPATH/project-common.sh"

get_destination_image_path() {
  image_name="$1"

  echo "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/$image_name:$images_tag"
}

stage_rsyslog_config() {
  src="$1"
  dest="$2"
  caption="$3"

  mkdir -p "$dest"/rsyslog
  cp -r "$src"/client/* "$dest"/rsyslog/
  cp "$src"/rsyslogCA.chain             "$dest"/rsyslog/rsyslog.d/
  cp "$src"/RSysLogrsyslog-client.chain "$dest"/rsyslog/rsyslog.d/
  cp "$src"/RSysLogrsyslog-client.key   "$dest"/rsyslog/rsyslog.d/
  echo "set \$.projectname = '$caption';" > "$dest"/rsyslog/rsyslog.d/10-pep-projectname.conf
}

build_config_dockerfile() {
  dockerfile="$1"

  img_name=$(basename "$dockerfile" .Dockerfile)
  dest_image=$(get_destination_image_path "$img_name")

  base_image=$("$SCRIPTPATH"/../scripts/gitlab-container-registry.sh \
    "$foss_root" "$api_key" get-image-location "$img_name" "$foss_sha")
  if has_foss_base_image "$img_name"; then
    if [ -z "$base_image" ]; then
      >&2 echo "Cannot find base image '$img_name' with SHA $foss_sha for $dest_image"
      return 1
    fi
    echo "Building $dest_image for dockerfile $dockerfile with base image $base_image"
  else
    echo "Building $dest_image for dockerfile $dockerfile without a base image"
  fi

  # Special handling for docker-compose image to expand CI variables in .env file
  if [ "$img_name" = "docker-compose" ]; then
    env_file="$env_config_dir/docker-compose/.env"

    if [ -f "$env_file" ]; then
      echo "Expanding variables in .env file..."

      # Create expanded .env file
      expanded_env_file=$(mktemp)
      cat "$env_file" | envsubst > "$expanded_env_file"

      # Copy the expanded .env file over the original for the Docker build
      mv "$expanded_env_file" "$env_file"

      echo "Variables expanded in .env file"
    else
      echo "No .env file found at $env_file"
    fi
  fi

  docker build -t "$dest_image" -f "$dockerfile" --pull \
    --build-arg "ENVIRONMENT=$environment" \
    --build-arg "PROJECT_DIR=$environment/project" \
    --build-arg "BASE_IMAGE=$base_image" \
    --build-arg "RSYSLOG_PREPOSITION=$rsyslog_preposition" \
    "$git_config_dir"

  docker push "$dest_image"
}

"$SCRIPTPATH"/../scripts/createConfigVersionJson.sh "$env_config_dir" "$env_project_dir" \
  > "$env_config_dir/configVersion.json"

rsyslog_preposition=without

if [ "$with_rsyslog" = "true" ]; then
  # Explicitly enable rsyslog when requested
  rsyslog_preposition=with
  if [ -d "$rsyslog_dir" ]; then
    echo "Enabling rsyslog configuration from $rsyslog_dir"
    caption=$(get_project_caption)
    stage_rsyslog_config "$rsyslog_dir" "$git_config_dir" "$caption"
  else
    >&2 echo "Warning: --with-rsyslog=true but rsyslog directory '$rsyslog_dir' does not exist."
    >&2 echo "Docker build will set RSYSLOG_PREPOSITION=with but no actual configuration is staged."
  fi
elif [ "$with_rsyslog" = "false" ]; then
  # Explicitly disable rsyslog when requested
  echo "Rsyslog configuration disabled by --with-rsyslog=false"
else
  echo "Rsyslog configuration disabled by default"
fi

docker_login

for file in $(config_dockerfiles_to_build); do
  build_config_dockerfile "$file"
done

built_client=$(config_dockerfiles_to_build | grep client || true)
if [ -n "$built_client" ]; then
  # Provide Docker credentials as environment variables to apptainer: see https://github.com/apptainer/singularity/issues/1302
  SINGULARITY_DOCKER_USERNAME="-" SINGULARITY_DOCKER_PASSWORD="$api_key" \
    apptainer build client.sif "docker://$(get_destination_image_path client)"
fi
