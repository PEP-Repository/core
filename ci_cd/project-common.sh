#!/usr/bin/env sh
# shellcheck disable=SC2154  # git_dir, environment, foss_dir, api_key are set by the sourcing script
# shellcheck disable=SC2034  # foss_image_names, foss_sha etc. are consumed by sourcing scripts

# Common helpers for PEP config-image scripts (provide-binaries.sh,
# build-config-images.sh, publish.sh).
#
# Source this after parsing arguments. Before sourcing, the sourcing script must
# define: SCRIPTPATH, git_dir, environment, foss_dir. For docker_login it must
# also define: api_key.
#
#   . "$SCRIPTPATH/project-common.sh"

# The FOSS base images that config images can be built on top of.
foss_image_names="docker-compose authserver_apache pep-monitoring pep-services client pep-scheduler pep-connector"

git_root=$(cd "$git_dir" && pwd)
git_config_dir="$git_root/config"
env_config_dir="$git_config_dir/$environment"
env_project_dir="$env_config_dir/project"
foss_root=$(cd "$git_root" && cd "$foss_dir" && pwd)
foss_sha=$("$SCRIPTPATH"/../scripts/gitdir.sh commit-sha "$foss_root")

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

has_foss_base_image() {
  name="$1"
  for candidate in $foss_image_names; do
    if [ "$name" = "$candidate" ]; then return 0; fi
  done
  return 1
}

docker_login() {
  echo "$api_key" | docker login "${CI_REGISTRY}" --username "-" --password-stdin
}
