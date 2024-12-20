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

command="$1"
git_dir="$2"
environment="$3"
images_tag="$4"
foss_dir="$5"
api_key_json_file="$6"
legacy_registry_location=${7:-}

foss_image_names="authserver_apache pep-monitoring pep-services client watchdog-watchdog"
# >&2 echo foss_image_names is "$foss_image_names"

git_root=$(cd "$git_dir" && pwd)
git_config_dir="$git_root/config"
env_config_dir="$git_config_dir/$environment"
env_project_dir="$env_config_dir/project"

foss_root=$(cd "$git_root" && cd "$foss_dir" && pwd)
# >&2 echo foss_root is "$foss_root"
foss_sha=$("$SCRIPTPATH"/../scripts/gitdir.sh commit-sha "$foss_root")
# >&2 echo foss_sha is "$foss_sha"
foss_host=$("$SCRIPTPATH"/../scripts/gitdir.sh origin-host "$foss_root")

dir_api() {
  dir="$1"
  shift
  
  "$SCRIPTPATH/../scripts/gitlab-api.sh" "$dir" "$api_key_json_file" "$@"
}

foss_api() {
  dir_api "$foss_root" "$@"
}

get_gitlab_registry() {
  dir="$1"
  
  dir_api "$dir" get "" | jq --raw-output ".container_registry_image_prefix"
}

images_root="$(get_gitlab_registry "$git_root")/$environment"

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
    if [ -d "$env_config_dir/$(basename "$file")" ] ; then
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
    name="$(basename "$file")"
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

run_foss_pipeline() {
  if [ -n "$foss_pipeline_id" ]; then
    >&2 echo "FOSS pipeline ($foss_pipeline_id) has already been run"
    return 1
  fi
  
  # Gitlab can only run pipelines for branches (or tags): see https://stackoverflow.com/a/63460457
  branchname="binaries_for_$images_tag"
  foss_api post "repository/branches?branch=$branchname&ref=$foss_sha" > /dev/null # Creating the branch will start a CI pipeline
  sleep 10 # Give Gitlab time to start the pipeline

  foss_pipeline_id=$(foss_api get "pipelines" | jq "[.[] | select(.ref==\"$branchname\")] | .[0].id")
  foss_project_path=$(gitlab_project_path "$foss_root")
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

get_docker_username() {
  "$SCRIPTPATH"/../scripts/gitlab-api-key-json.sh user "$api_key_json_file"
}

get_docker_api_key() {
  "$SCRIPTPATH"/../scripts/gitlab-api-key-json.sh key "$api_key_json_file"
}

docker_login() {
  foss_registry=$(get_gitlab_registry "$foss_root")
  # >&2 echo foss_registry is "$foss_registry"

  api_user=$(get_docker_username)
  api_key=$(get_docker_api_key)
  reg_host=$(echo "$foss_registry" | cut -d/ -f1)
  # >&2 echo reg_host is "$reg_host"
  # TODO: verify that host for $git_root is the same

  docker login -u "$api_user" -p "$api_key" "$reg_host"
}

provide_base_images() {
  # Provide only the base images needed to build config images.
  provide_images=$(config_images_with_foss_base)
  if [ -z "$provide_images" ]; then
    # No base images required to build config images.
    # This may be the pep/ops "release" branch, which should provide all base images.
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
  
  echo "$images_root/$image_name:$images_tag"
}

build_config_dockerfile() {
  dockerfile="$1"
  
  image_name=$(basename "$dockerfile")
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
  
  # Only build "with" rsyslog integration if caller provides a <git-root>/config/rsyslog directory
  rsyslog_preposition=without
  if [ -d "$git_config_dir/rsyslog" ]; then
    rsyslog_preposition=with
  fi
  
  docker build -t "$dest_image" -f "$dockerfile" --pull --build-arg "ENVIRONMENT=$environment" --build-arg "PROJECT_DIR=$environment/project" --build-arg "BASE_IMAGE=$base_image" --build-arg "RSYSLOG_PREPOSITION=$rsyslog_preposition" "$git_config_dir"
  docker push "$dest_image"
  
}

build_config_images() {
  "$SCRIPTPATH/../scripts/createConfigVersionJson.sh" "$env_config_dir" "$env_project_dir" > "$env_config_dir/configVersion.json"
  
  docker_login
  
  for file in $(config_dockerfiles_to_build)
  do
    build_config_dockerfile "$file"
  done
  
  built_client=$(config_dockerfiles_to_build | grep client || true)
  if [ -n "$built_client" ]; then
    # Provide Docker credentials as environment variables to apptainer: see https://github.com/apptainer/singularity/issues/1302
    SINGULARITY_DOCKER_USERNAME=$(get_docker_username) SINGULARITY_DOCKER_PASSWORD=$(get_docker_api_key) \
      apptainer build client.sif "docker://$(get_destination_image_path client)"
  fi
}

update_latest_tags() {
  docker_login
  
  names="$(config_dockerfiles_to_build | while read -r file; do basename "$file"; done)"
  # >&2 echo "Names is $names"
  
  for name in $names
  do
    docker buildx imagetools create --tag "$images_root"/"$name":latest "$images_root"/"$name":"$images_tag"
    if [ -n "$legacy_registry_location" ]; then
      docker buildx imagetools create --tag "$legacy_registry_location"/"$name":latest "$images_root"/"$name":latest
    fi
  done
}

publish_client() {
  docker_login

  caption=$(cat "$env_project_dir/caption.txt")
  
  docker buildx imagetools create --tag gitlabregistry.pep.cs.ru.nl/pep-public/core/"$caption"-"$environment":latest "$images_root"/client:"$images_tag"
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
  *)
    >&2 echo Unsupported command "$command"
    exit 1
esac
