#!/usr/bin/env sh
# shellcheck disable=SC2154  # git_dir, environment, foss_dir, api_key are set by the sourcing script
# shellcheck disable=SC2034  # foss_sha etc. are consumed by sourcing scripts

# Common helpers for project scripts, such as:
# provide-binaries.sh, config-dockerfiles.sh, publish.sh.
#
# Source this after parsing arguments and after defining: `SCRIPTPATH`, `git_dir`, `environment`, `foss_dir`.
# The `docker_login` function requires `api_key` to be defined as well

# shellcheck source=scripts/sh-utils.sh
. "$SCRIPTPATH/../scripts/sh-utils.sh"

git_root=$(cd "$git_dir" && pwd)
readonly git_root
readonly git_config_dir="$git_root/config"
readonly env_config_dir="$git_config_dir/$environment"
readonly env_project_dir="$env_config_dir/project"
foss_root=$(cd "$git_root" && cd "$foss_dir" && pwd)
readonly foss_root
foss_sha=$("$SCRIPTPATH"/../scripts/gitdir.sh commit-sha "$foss_root")
readonly foss_sha

get_project_caption() {
  if [ -f "$env_project_dir/caption.txt" ]; then
    cat "$env_project_dir/caption.txt"
  else
    >&2 echo "Error: caption.txt not found in $env_project_dir"
    exit 1
  fi
}

all_config_dockerfiles() {
  find "$foss_root"/docker/config-dockerfiles -type f
}

config_dockerfiles_to_build() {
  all_config_dockerfiles | while read -r file; do
    basename_no_ext=$(basename "$file" .Dockerfile)
    # If a config Dockerfile in the FOSS repo has a matching subdirectory in the
    # project repo's config directory, it should be built for that project.
    if [ -d "$env_config_dir/$basename_no_ext" ]; then
      echo "$file"
    fi
  done
}

# The names of the FOSS base images that config images can be built on top of.
# A config Dockerfile builds on top of a FOSS base image iff it declares a BASE_IMAGE
# arg; its basename matches the FOSS image name.
foss_image_names() {
  all_config_dockerfiles | while read -r file; do
    if grep -q 'ARG BASE_IMAGE' "$file"; then
      basename "$file" .Dockerfile
    fi
  done
}

has_foss_base_image() {
  list_contains "$(foss_image_names)" "$1"
}

docker_login() {
  echo "$api_key" | docker login "${CI_REGISTRY}" --username "-" --password-stdin
}
