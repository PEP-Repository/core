#!/usr/bin/env sh

# Manages Docker image publishing for PEP project repositories.
# Commands: update-latest, publish-client, publish

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

command="${1:?Expected command (update-latest|publish-client|publish)}"; shift

git_dir=""
environment=""
images_tag=""
foss_dir=""
api_key=""
image_name=""

usage() {
  exit_code=${1:-1}
  cat << EOF
Usage: $0 <command> [OPTIONS]

Commands:
  update-latest    Retag pipeline-tagged images as :latest in the project registry
  publish-client   Publish the client image to the public registry
  publish          Publish a named image to the public registry (requires --image)

Options:
  -g, --git-dir DIR        Git directory
  -e, --env ENV            Environment name
  -t, --tag TAG            Pipeline tag for images
  -f, --foss-dir DIR       FOSS directory
  -k, --api-key KEY        Registry API token (not needed for update-latest)
  -i, --image NAME         Image name (required for publish)
  -h, --help               Show this help message
EOF
  exit "$exit_code"
}

while [ $# -gt 0 ]; do
  case "$1" in
    -g|--git-dir)   shift; git_dir="$1" ;;
    -e|--env)       shift; environment="$1" ;;
    -t|--tag)       shift; images_tag="$1" ;;
    -f|--foss-dir)  shift; foss_dir="$1" ;;
    -k|--api-key)   shift; api_key="$1" ;;
    -i|--image)     shift; image_name="$1" ;;
    -h|--help)      usage 0 ;;
    *) >&2 echo "Unknown option: $1"; usage 1 ;;
  esac
  shift
done

if [ -z "$git_dir" ] || [ -z "$environment" ] || [ -z "$images_tag" ] || [ -z "$foss_dir" ]; then
  >&2 echo "Error: Missing required parameters."
  usage 1
fi

if [ "$command" != "update-latest" ] && [ -z "$api_key" ]; then
  >&2 echo "Error: Missing API key."
  usage 1
fi

if [ "$command" = "publish" ] && [ -z "$image_name" ]; then
  >&2 echo "Error: The publish command requires --image."
  usage 1
fi

# shellcheck source=ci_cd/project-common.sh
. "$SCRIPTPATH/project-common.sh"

update_latest_tags() {
  # CI_REGISTRY_PASSWORD only allows pushes to this project's registry
  echo "$CI_REGISTRY_PASSWORD" | docker login "$CI_REGISTRY" --username "$CI_REGISTRY_USER" --password-stdin

  names="$(config_dockerfiles_to_build | while read -r file; do basename "$file" .Dockerfile; done)"

  for name in $names; do
    docker buildx imagetools create \
      --tag "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/${name}:latest" \
      "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/${name}:${images_tag}"
  done
}

do_publish() {
  image_target="$1"
  image_source="$2"

  docker_login
  docker buildx imagetools create \
    --tag "${CI_REGISTRY}/pep-public/core/${image_target}" \
    "${image_source}"
}

publish_client() {
  caption=$(get_project_caption)
  do_publish "${caption}-${environment}:latest" \
    "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/client:${images_tag}"
}

publish_image() {
  caption=$(get_project_caption)
  do_publish "${image_name}-${caption}-${environment}:latest" \
    "${CI_REGISTRY_IMAGE}/${CI_COMMIT_REF_NAME}/${image_name}:${images_tag}"
}

case $command in
  update-latest)  update_latest_tags ;;
  publish-client) publish_client ;;
  publish)        publish_image ;;
  *)
    >&2 echo "Unsupported command: $command"
    usage 1
    ;;
esac
