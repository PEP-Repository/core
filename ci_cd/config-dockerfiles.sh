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

if [ "$command" != "update-latest" ] && [ -z "$api_key" ]; then
  echo "Error: Missing API key."
  usage 1
fi

# Verify image_name parameter for the "publish" command
if [ "$command" = "publish" ] && [ -z "$image_name" ]; then
  echo "Error: The publish command requires an image name (--image)."
  usage 1
fi

# Verify rsyslog parameters
if [ "$with_rsyslog" = "true" ] && [ -z "$rsyslog_dir" ]; then
  echo "Error: --with-rsyslog=true but no rsyslog directory (--rsyslog-dir) specified."
  usage 1
fi

foss_image_names="docker-compose authserver_apache pep-monitoring pep-services client pep-scheduler pep-connector"

git_root=$(cd "$git_dir" && pwd)
git_config_dir="$git_root/config"
env_config_dir="$git_config_dir/$environment"
env_project_dir="$env_config_dir/project"

get_project_caption() {
  if [ -f "$env_project_dir/caption.txt" ]; then
    cat "$env_project_dir/caption.txt"
  else
    >&2 echo "Error: caption.txt not found in $env_project_dir"
    exit 1
  fi
}

foss_root=$(cd "$git_root" && cd "$foss_dir" && pwd)
# >&2 echo foss_root is "$foss_root"
foss_sha=$("$SCRIPTPATH"/../scripts/gitdir.sh commit-sha "$foss_root")
# >&2 echo foss_sha is "$foss_sha"
foss_host=$("$SCRIPTPATH"/../scripts/gitdir.sh origin-host "$foss_root")

dir_api() {
  dir="$1"
  shift
  
  "$SCRIPTPATH/../scripts/gitlab-api.sh" "$dir" "$api_key" "$@"
}

foss_api() {
  dir_api "$foss_root" "$@"
}

get_gitlab_registry() {
  dir="$1"
  
  dir_api "$dir" get "" | jq --raw-output ".container_registry_image_prefix"
}

# Invoke with JSON containing a ".created_at" property, e.g.
# - Docker image (tag) details: https://docs.gitlab.com/ee/api/container_registry.html#get-details-of-a-registry-repository-tag
# - project package details: https://docs.gitlab.com/ee/api/packages.html#get-a-project-package
# - pipelines
# If the ".created_at" is older than the (hard-coded) threshold, this function echoes
# the value of that ".created_at" property (so that the caller can report its value).
# For more recent values of the ".created_at" property, this function does nothing.
get_outdated_creation_timestamp() {
  entry="$1"

  created_at=$(echo "$entry" | jq --raw-output ".created_at")
  # >&2 echo "created_at is $created_at"
  seconds=$(( $(date +%s) - $(date -d "$created_at" +%s)))
  # >&2 echo "seconds is $seconds"
  days=$(( seconds / 60 / 60 / 24 ))
  # >&2 echo "days is $days"

  if [ "$days" -ge 6 ]; then
    echo "$created_at"
  fi
}

# Invoke as (external) command: myloc=$(get_foss_image_location some_image_name)
get_foss_image_location() {
  imgname="$1"
  # >&2 echo "get_foss_image_location($imgname)"

  repos=$(foss_api get-multipage "registry/repositories")
  # >&2 echo "repos is $repos"
  repo=$(echo "$repos" | jq ".[] | select(.name==\"$imgname\")")
  # >&2 echo "repo is $repo"
  if [ -z "$repo" ]; then
    >&2 echo No FOSS Docker images found with name "$imgname".
    return
  fi
  
  repoid=$(echo "$repo" | jq ".id")
  # >&2 echo "repoid is $repoid"

  details=$(foss_api get "registry/repositories/$repoid/tags/$foss_sha" || true)
  # >&2 echo "details is $details"
  if [ -z "$details" ]; then
    >&2 echo FOSS Docker image "$imgname" not found for SHA "$foss_sha".
    return
  fi
  created_at=$(get_outdated_creation_timestamp "$details")
  if [ -n "$created_at" ]; then
    >&2 echo FOSS Docker image "$imgname" for SHA "$foss_sha" is outdated \(created at "$created_at"\).
    return;
  fi
  
  echo "$details" | jq --raw-output ".location"
}

all_config_dockerfiles() {
  find "$foss_root"/docker/config-dockerfiles -type f
}

config_dockerfiles_to_build() {
  all_config_dockerfiles | while read -r file
  do
    basename_no_ext=$(basename "$file" .Dockerfile)
    # If the dockerfiles in foss/docker/config-dockerfiles are also directories in the project's
    # config directory, they are the dockerfiles that should be built for that project.
    if [ -d "$env_config_dir/$basename_no_ext" ] ; then
      echo "$file"
    fi
  done
}

has_foss_base_image() {
  name="$1"
  
  for candidate in $foss_image_names
  do
    if [ "$name" = "$candidate" ] ; then
      return 0
    fi
  done
  
  return 1
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

# TODO: consolidate duplicate code with gitlab-api.sh
contains() {
  string="$1"
  substring="$2"
  if [ "${string#*"$substring"}" != "$string" ]
  then
    return 0
  else
    return 1
  fi
}

gitlab_project_path() {
  dir="$1"
  
  "$SCRIPTPATH"/../scripts/gitdir.sh origin-path "$dir"
}

foss_pipeline_id=

get_foss_pipeline_id() {
  # Retry obtaining the pipeline ID for the given branch name, with exponential backoff.

  branchname="$1"
  max_retries=5
  retry_delay=5
  attempt=1

  while [ $attempt -le $max_retries ]; do

    # Initial wait for GitLab to start the pipeline
    sleep $retry_delay

    id=$(foss_api get "pipelines" | jq -r "[.[] | select(.ref==\"$branchname\")] | .[0].id")
    if [ "$id" != "null" ] && [ -n "$id" ]; then
      echo "$id"
      return 0
    fi
    >&2 echo "Pipeline for branch $branchname not found (attempt $attempt/$max_retries), retrying in $retry_delay seconds..."

    retry_delay=$((retry_delay * 2))
    attempt=$((attempt + 1))
  done

  >&2 echo "Failed to obtain pipeline ID for branch $branchname after $max_retries attempts."
  return 1
}

run_foss_pipeline() {
  if [ -n "$foss_pipeline_id" ]; then
    >&2 echo "FOSS pipeline ($foss_pipeline_id) has already been run"
    return 1
  fi
  
  # Gitlab can only run pipelines for branches (or tags): see https://stackoverflow.com/a/63460457
  branchname="binaries_for_$images_tag"
  foss_api post "repository/branches?branch=$branchname&ref=$foss_sha" > /dev/null # Creating the branch will start a CI pipeline

  foss_pipeline_id=$(get_foss_pipeline_id "$branchname") || return 1
  foss_project_path=$(gitlab_project_path "$foss_root")

  # Set descriptive pipeline name
  foss_api put "pipelines/$foss_pipeline_id/metadata" --data "name=Providing binaries for $CI_PROJECT_PATH/$CI_COMMIT_REF_NAME" > /dev/null || true

  echo "Running pipeline $foss_pipeline_id in project $foss_project_path for branch $branchname: "\
    "https://$foss_host/$foss_project_path/-/pipelines/$foss_pipeline_id"
  
  # All possible statuses are documented on https://docs.gitlab.com/ee/api/pipelines.html. I cannot find any documentation on what these statuses mean.
  # Not all statuses are listed below. I don't expect we will encounter the missing statuses, but if we do we must investigate in which category they should fall.
  running_statuses="\"pending\" \"running\" \"created\" \"preparing\" \"waiting_for_resource\""
  success_statuses="\"success\" \"skipped\""
  failure_statuses="\"failed\" \"canceled\" \"canceling\""
  
  pipeline_result=
  while [ -z "$pipeline_result" ]
  do
    status=$(foss_api get "pipelines/${foss_pipeline_id}" | jq ".status")
  
    if contains "$success_statuses" "$status"
    then
      echo "Pipeline succeeded with status: '$status'"
      pipeline_result=0
    elif contains "$failure_statuses" "$status"
    then
      >&2 echo "Pipeline failed with status: '$status'"
      pipeline_result=1
    elif ! contains "$running_statuses" "$status"
    then
      >&2 echo "Received unsupported status \"$status\" from Gitlab API"
      pipeline_result=1
    fi
  
    sleep 30
  done
  
  foss_api delete "repository/branches/$branchname" # Clean up: remove branch
  
  return $pipeline_result
}

provide_foss_image() {
  name="$1"

  location="$(get_foss_image_location "$name")"
  if [ -z "$location" ] ; then
    echo Running a FOSS pipeline to \(re-\)produce Docker image "$name"...
    run_foss_pipeline
    location="$(get_foss_image_location "$name")"
    if [ -z "$location" ] ; then
      >&2 echo FOSS pipeline did not produce expected Docker image "$name" for SHA "$foss_sha"
      return 1
    fi
  else
    echo FOSS Docker image found at "$location"
  fi
}

get_foss_package_file_id() {
  package_name="$1"
  file_name="$2"

  package=$(foss_api get "packages?package_name=$package_name&package_type=generic&package_version=$foss_sha" | jq first)
  # >&2 echo "package is $package"

  if [ -z "$package" ]; then
    >&2 echo "FOSS package '$package_name' not found for SHA $foss_sha."
    return
  fi

  package_id=$(echo "$package" | jq ".id")
  # >&2 echo "package_id is $package_id"
  files=$(foss_api get-multipage "packages/$package_id/package_files" | jq --arg file_name "$file_name" --compact-output '.[] | select( .file_name == $file_name ) | { created_at, id }' | sort -r)
  # >&2 echo "files is $files"
  
  if [ -z "$files" ]; then
    >&2 echo "No file named '$file_name' found in FOSS package '$package_name' for SHA $foss_sha."
    return
  fi

  file=$(echo "$files" | head -n 1)
  # >&2 echo "file is $file"
  created_at=$(get_outdated_creation_timestamp "$file")
  
  if [ -n "$created_at" ]; then
    >&2 echo File "$file_name" in FOSS package "$package_name" for SHA "$foss_sha" is outdated \(created at "$created_at"\).
    return;
  fi

  echo "$file" | jq ".id"
}

download_foss_package() {
  package_name="$1"
  file_name="$2"
  file_id=$(get_foss_package_file_id "$package_name" "$file_name")
  if [ -z "$file_id" ]; then
    echo "Running a FOSS pipeline to (re-)produce file '$file_name' in package '$package_name' for SHA $foss_sha..."
    run_foss_pipeline
    file_id=$(get_foss_package_file_id "$package_name" "$file_name")
    if [ -z "$file_id" ]; then
      >&2 echo "FOSS pipeline did not produce expected file '$file_name' in '$package_name' package for SHA $foss_sha"
      return 1
    fi
  fi

  # Unfortunately there doesn't seem to be an API endpoint to retrieve (download) the file by ID.
  # But according to https://docs.gitlab.com/ee/user/packages/generic_packages/#download-package-file ,
  # "the most recent one is retrieved" when retrieving by name (as we do below), which should be
  # the one with the file_id that we determined.
  foss_api get "packages/generic/$package_name/$foss_sha/$file_name" --output "$file_name"
  echo Downloaded FOSS package file "$file_id" from "packages/generic/$package_name/$foss_sha/$file_name".
}

docker_login() {
  echo "$api_key" | docker login "${CI_REGISTRY}" -u "-" --password-stdin
}

provide_base_images() {
  if [ "$force_rebuild" = "true" ]; then
    run_foss_pipeline
  fi
  # Provide only the base images needed to build config images.
  provide_images=$(config_images_with_foss_base)
  if [ -z "$provide_images" ]; then
    # No base images required to build config images.
    # This may be a pep/ops "release-X.Y" branch, which should provide all base images.
    provide_images=$foss_image_names
  fi
  
  for name in $provide_images
  do
    provide_foss_image "$name"
  done
  
  download_foss_package wixlibrary pepBinaries.wixlib
  download_foss_package macos-x86-bins macOS_x86_64_bins.zip
  download_foss_package macos-arm-bins macOS_arm64_bins.zip
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
  
  base_image=$(get_foss_image_location "$image_name")
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

update_latest_tags() {
  # CI_REGISTRY_PASSWORD only allows pushes to this projects registry
  docker login -u "$CI_REGISTRY_USER" -p "$CI_REGISTRY_PASSWORD" "$CI_REGISTRY"

  names="$(config_dockerfiles_to_build | while read -r file; do basename "$file" .Dockerfile; done)"
  # >&2 echo "Names is $names"

  for name in $names
  do
    docker buildx imagetools create --tag "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/${name}:latest" "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/${name}:${images_tag}"
  done
}

publish() {
  image_target="$1"
  image_source="$2"

  docker_login

  docker buildx imagetools create --tag "${CI_REGISTRY}/pep-public/core/${image_target}" "${image_source}"
}

publish_client() {
  caption=$(get_project_caption)
  publish "${caption}-${environment}:latest" "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/client:${images_tag}"
}

publish_image() {
  caption=$(get_project_caption)
  publish "${image_name}-${caption}-${environment}:latest" "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/${image_name}:${images_tag}"
}

case $command in
  provide-base-images)
    provide_base_images
    ;;
  build)
    build_config_images
    ;;
  update-latest)
    update_latest_tags
    ;;
  publish-client)
    publish_client
    ;;
  publish)
    publish_image
    ;;
  *)
    >&2 echo "Unsupported command: $command"
    usage 1
esac
