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
# image in the source repo's registry. If the registry does not contain an image
# for the required version (i.e. the commit SHA of the PEP source submodule),
# this script can also run a pipeline in the source repo to produce the required
# base image.

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

# Portable envsubst replacement (But beware: also replaces shell variables and allows command injections)
envsubst() { eval "echo \"$(sed 's/\\/\\\\/g; s/"/\\"/g')\""; }

# Default values
command=""
git_dir=""
environment=""
images_tag=""
foss_dir=""
api_key=""
image_name=""
with_rsyslog=""
rsyslog_dir=""
force_rebuild="false"

usage() {
  exit_code=${1:-1}
  cat << EOF
Usage: $0 [OPTIONS]

Required parameters:
  -c, --command COMMAND    Operation to perform
                          (provide-base-images|build|update-latest|publish-client|publish)
  -g, --git-dir DIR        Git directory
  -e, --env ENV            Environment name
  -t, --tag TAG            Tag for images
  -f, --foss-dir DIR       FOSS directory

Optional parameters:
  -k, --api-key            GitLab API key, provide-base-images and build require a full API token, publish commands require a read/write registry token
  -b, --force-rebuild BOOL Force rebuild of base images (true|false), default: false
  -i, --image NAME         Image name (only used by the publish command)
  -r, --with-rsyslog BOOL  Force enable/disable rsyslog (true|false, overrides directory check)
  -d, --rsyslog-dir DIR    Custom rsyslog directory
  -h, --help               Show this help message
EOF
  exit "$exit_code"
}

while [ $# -gt 0 ]; do
  case "$1" in
    -b|--force-rebuild)
      shift
      force_rebuild="$1"
      ;;
    -c|--command)
      shift
      command="$1"
      ;;
    -g|--git-dir)
      shift
      git_dir="$1"
      ;;
    -e|--env)
      shift
      environment="$1"
      ;;
    -t|--tag)
      shift
      images_tag="$1"
      ;;
    -f|--foss-dir)
      shift
      foss_dir="$1"
      ;;
    -k|--api-key)
      shift
      api_key="$1"
      ;;
    -i|--image)
      shift
      image_name="$1"
      ;;
    -d|--rsyslog-dir)
      shift
      rsyslog_dir="$1"
      ;;
    -r|--with-rsyslog)
      shift
      with_rsyslog="$1"
      ;;
    -h|--help)
      usage 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage 1
      ;;
  esac
  shift
done

# Verify required parameters for all commands
if [ -z "$command" ] || [ -z "$git_dir" ] || [ -z "$environment" ] || [ -z "$images_tag" ] || [ -z "$foss_dir" ]; then
  echo "Error: Missing required parameters."
  usage 1
fi

# Verify rsyslog parameters
if [ "$with_rsyslog" = "true" ] && [ -z "$rsyslog_dir" ]; then
  echo "Error: --with-rsyslog=true but no rsyslog directory (--rsyslog-dir) specified."
  usage 1
fi

# shellcheck source=ci_cd/project-common.sh
. "$SCRIPTPATH/project-common.sh"

dir_api() {
  dir="$1"
  shift
  
  "$SCRIPTPATH/../scripts/gitlab-api.sh" "$dir" "$api_key" "$@"
}

foss_api() {
  dir_api "$foss_root" "$@"
}

is_outdated() {
  printf '%s' "$1" | "$SCRIPTPATH/../scripts/gitlab-api.sh" "$git_dir" "$api_key" get-outdated-creation-timestamp
}

config_images_with_foss_base() {
  config_dockerfiles_to_build | while read -r file
  do
    name="$(basename "$file" .Dockerfile)"
    if has_foss_base_image "$name" ; then
      echo "$name"
    fi
  done
}

ensure_pipeline_triggered() {
  if $pipeline_triggered; then return; fi
  pipeline_triggered=true
  pipeline_label="Providing binaries for ${CI_PROJECT_PATH:-unknown}/${CI_COMMIT_REF_NAME:-unknown}"
  "$SCRIPTPATH"/../scripts/gitlab-pipeline.sh "$foss_root" "$api_key" \
    trigger-and-wait "binaries_for_$images_tag" "$foss_sha" "$pipeline_label"
}

provide_foss_image() {
  name="$1"

  location=$("$SCRIPTPATH"/../scripts/gitlab-container-registry.sh \
    "$foss_root" "$api_key" get-image-location "$name" "$foss_sha")
  if [ -z "$location" ] ; then
    echo Running a FOSS pipeline to \(re-\)produce Docker image "$name"...
    ensure_pipeline_triggered
    location=$("$SCRIPTPATH"/../scripts/gitlab-container-registry.sh \
      "$foss_root" "$api_key" get-image-location "$name" "$foss_sha")
    if [ -z "$location" ] ; then
      >&2 echo FOSS pipeline did not produce expected Docker image "$name" for SHA "$foss_sha"
      return 1
    fi
  else
    echo FOSS Docker image found at "$location"
  fi
}

download_foss_package() {
  package_name="$1"
  file_name="$2"
  if ! "$SCRIPTPATH"/../scripts/gitlab-packages.sh "$foss_root" "$api_key" \
      download-generic "$package_name" "$foss_sha" "$file_name"; then
    echo "Running a FOSS pipeline to (re-)produce file '$file_name' in package '$package_name' for SHA $foss_sha..."
    ensure_pipeline_triggered
    "$SCRIPTPATH"/../scripts/gitlab-packages.sh "$foss_root" "$api_key" \
      download-generic "$package_name" "$foss_sha" "$file_name" || {
      >&2 echo "FOSS pipeline did not produce expected file '$file_name' in '$package_name' for SHA $foss_sha"
      return 1
    }
  fi
}

provide_base_images() {
  if [ "$force_rebuild" = "true" ]; then
    ensure_pipeline_triggered
  fi
  # Provide only the base images needed to build config images.
  provide_images=$(config_images_with_foss_base)
  if [ -z "$provide_images" ]; then
    # No base images required to build config images.
    provide_images=$foss_image_names
  fi
  for name in $provide_images; do
    provide_foss_image "$name"
  done

  download_foss_package wixlibrary pepBinaries.wixlib
  download_foss_package macos-x86_64-bins macOS_x86_64_bins.zip
  download_foss_package macos-arm64-bins macOS_arm64_bins.zip
  download_foss_package flatpak pep.flatpak
}

get_destination_image_path() {
  image_name="$1"
  
  echo "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/$image_name:$images_tag"
}

build_config_dockerfile() {
  dockerfile="$1"
  
  image_name=$(basename "$dockerfile" .Dockerfile)
  dest_image=$(get_destination_image_path "$image_name")
  
  base_image=$("$SCRIPTPATH"/../scripts/gitlab-container-registry.sh \
    "$foss_root" "$api_key" get-image-location "$image_name" "$foss_sha")
  if has_foss_base_image "$image_name" ; then
    if [ -z "$base_image" ]; then
      >&2 echo Cannot find base image "$image_name" with SHA "$foss_sha" for "$dest_image"
      return 1
    fi
    echo "Building $dest_image for dockerfile $dockerfile with base image $base_image"
  else
    echo "Building $dest_image for dockerfile $dockerfile without a base image"
  fi

  # Special handling for docker-compose image to expand CI variables in .env file
  if [ "$image_name" = "docker-compose" ]; then
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

stage_rsyslog_config() {
  src="$1"
  dest="$2"
  caption="$3"

  # Create destination (sub)directory
  mkdir -p "$dest"/rsyslog

  # Copy (client) configuration files from source to destination
  cp -r "$src"/client/* "$dest"/rsyslog/

  # Copy PKI files from source to destination
  cp "$src"/rsyslogCA.chain             "$dest"/rsyslog/rsyslog.d/
  cp "$src"/RSysLogrsyslog-client.chain "$dest"/rsyslog/rsyslog.d/
  cp "$src"/RSysLogrsyslog-client.key   "$dest"/rsyslog/rsyslog.d/

  # Create config file with caption (usually project abbreviation)
  echo "set \$.projectname = '$caption';" > "$dest"/rsyslog/rsyslog.d/10-pep-projectname.conf
}

build_config_images() {
  "$SCRIPTPATH/../scripts/createConfigVersionJson.sh" "$env_config_dir" "$env_project_dir" > "$env_config_dir/configVersion.json"
  
  # Set up rsyslog configuration
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
  
  for file in $(config_dockerfiles_to_build)
  do
    build_config_dockerfile "$file"
  done
  
  built_client=$(config_dockerfiles_to_build | grep client || true)
  if [ -n "$built_client" ]; then
    # Provide Docker credentials as environment variables to apptainer: see https://github.com/apptainer/singularity/issues/1302
    SINGULARITY_DOCKER_USERNAME="-" SINGULARITY_DOCKER_PASSWORD="$api_key" \
      apptainer build client.sif "docker://$(get_destination_image_path client)"
  fi
}

case $command in
  provide-base-images)
    provide_base_images
    ;;
  build)
    build_config_images
    ;;
  *)
    >&2 echo "Unsupported command: $command"
    usage 1
esac
