#!/usr/bin/env sh

# Downloads FOSS Docker images and binary packages needed by PEP project repositories.
# Triggers a FOSS CI pipeline to (re-)produce required artifacts that are missing or outdated.

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

git_dir=""
environment=""
images_tag=""
foss_dir=""
api_key=""
force_rebuild="false"

usage() {
  exit_code=${1:-1}
  cat << EOF
Usage: $0 [OPTIONS]

Options:
  -g, --git-dir DIR        Git directory
  -e, --env ENV            Environment name
  -t, --tag TAG            Pipeline tag
  -f, --foss-dir DIR       FOSS directory
  -k, --api-key KEY        GitLab API token
  -b, --force-rebuild BOOL Force rebuild via FOSS pipeline (true|false, default: false)
  -h, --help               Show this help message
EOF
  exit "$exit_code"
}

while [ $# -gt 0 ]; do
  case "$1" in
    -g|--git-dir)        shift; git_dir="$1" ;;
    -e|--env)            shift; environment="$1" ;;
    -t|--tag)            shift; images_tag="$1" ;;
    -f|--foss-dir)       shift; foss_dir="$1" ;;
    -k|--api-key)        shift; api_key="$1" ;;
    -b|--force-rebuild)  shift; force_rebuild="$1" ;;
    -h|--help)           usage 0 ;;
    *) >&2 echo "Unknown option: $1"; usage 1 ;;
  esac
  shift
done

if [ -z "$git_dir" ] || [ -z "$environment" ] || [ -z "$images_tag" ] || [ -z "$foss_dir" ] || [ -z "$api_key" ]; then
  >&2 echo "Error: Missing required parameters."
  usage 1
fi

# shellcheck source=ci_cd/project-common.sh
. "$SCRIPTPATH/project-common.sh"

config_images_with_foss_base() {
  all_config_dockerfiles | while read -r file; do
    name="$(basename "$file" .Dockerfile)"
    if has_foss_base_image "$name" && [ -d "$env_config_dir/$name" ]; then
      echo "$name"
    fi
  done
}

pipeline_triggered=false

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
  if [ -z "$location" ]; then
    echo "Running a FOSS pipeline to (re-)produce Docker image '$name'..."
    ensure_pipeline_triggered
    location=$("$SCRIPTPATH"/../scripts/gitlab-container-registry.sh \
      "$foss_root" "$api_key" get-image-location "$name" "$foss_sha")
    if [ -z "$location" ]; then
      >&2 echo "FOSS pipeline did not produce expected Docker image '$name' for SHA $foss_sha"
      return 1
    fi
  else
    echo "FOSS Docker image found at $location"
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

if [ "$force_rebuild" = "true" ]; then
  ensure_pipeline_triggered
fi

# Determine which FOSS base images to provide
provide_images=$(config_images_with_foss_base)
if [ -z "$provide_images" ]; then
  provide_images=$(foss_image_names)
fi

for name in $provide_images; do
  provide_foss_image "$name"
done

download_foss_package wixlibrary pepBinaries.wixlib
download_foss_package macos-x86_64-bins macOS_x86_64_bins.zip
download_foss_package macos-arm64-bins macOS_arm64_bins.zip
download_foss_package flatpak pep.flatpak
