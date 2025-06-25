#!/usr/bin/env sh

# Performs actions on the Gitlab container registry.
#
# Note that repository deletion (i.e. the "clean-for-branch" command) requires
# an API key file for a project _owner_, so the support key from the pep/ops
# repository can't be used.

set -o errexit
set -o nounset

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

git_dir="$1"
api_key="$2"
command="$3"

list_repositories() {
  "$SCRIPTPATH"/gitlab-api.sh "$git_dir" "$api_key" get-multipage registry/repositories | jq --compact-output ".[]"
}

get_repository_names() {
  entries="$1"
  echo "$entries" | jq -r ".name" | sort
}

get_branch_names() {
  repositories="$1"
  echo "$repositories" | cut -d/ -f1
}

list_branches() {
  entries=$(list_repositories)
  repositories=$(get_repository_names "$entries")
  get_branch_names "$repositories" | uniq
}

delete_for_branch() {
  branch="$1"
  list_repositories | while read -r repo_json;
  do
    repository=$(get_repository_names "$repo_json")
    repo_branch=$(get_branch_names "$repository")
    if [ "$repo_branch" = "$branch" ]; then
      id=$(echo "$repo_json" | jq -r ".id")
      echo "$repository"
      error=$("$SCRIPTPATH"/gitlab-api.sh "$git_dir" "$api_key" delete "registry/repositories/$id" 2>&1)
      # A HTTP 202 "Accepted" code indicates that the API will delete the repository asynchronously:
      # see https://docs.gitlab.com/ee/api/container_registry.html#delete-registry-repository
      # Suppress this status (since it indicates success) and report all others.
      if [ "$error" != "202" ]; then
        >&2 echo "$error"
      fi
    fi
  done
}

# TODO: support aNyCaSeCoMmAnDs
case $command in
  list-branches)
    list_branches
    ;;
  delete-for-branch)
    delete_for_branch "$4"
    ;;
  *)
    >&2 echo Unsupported command "$command"
    exit 1
    ;;
esac
