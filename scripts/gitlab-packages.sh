#!/usr/bin/env sh

# Downloads packages from the GitLab package registry.

set -eu

readonly SCRIPTSELF=$(command -v "$0")
readonly SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

readonly git_dir="${1:?Expected git dir}"; shift
readonly api_key="${1:?Expected API key}"; shift
readonly command="${1:?Expected command}"; shift

gitlab_api() {
  "$SCRIPTPATH"/gitlab-api.sh "$git_dir" "$api_key" "$@"
}

is_outdated() {
  printf '%s' "$1" | "$SCRIPTPATH"/gitlab-api.sh "$git_dir" "$api_key" get-outdated-creation-timestamp
}

get_generic_file_id() {
  package_name="$1"
  sha="$2"
  file_name="$3"

  package=$(gitlab_api get "packages?package_name=$package_name&package_type=generic&package_version=$sha" | jq first)
  if [ -z "$package" ] || [ "$package" = "null" ]; then
    >&2 echo "FOSS package '$package_name' not found for SHA $sha."
    return
  fi

  package_id=$(printf '%s' "$package" | jq ".id")
  files=$(gitlab_api get-multipage "packages/$package_id/package_files" \
    | jq --arg file_name "$file_name" --compact-output '.[] | select( .file_name == $file_name ) | { created_at, id }' \
    | sort -r)

  if [ -z "$files" ]; then
    >&2 echo "No file named '$file_name' found in FOSS package '$package_name' for SHA $sha."
    return
  fi

  file=$(printf '%s' "$files" | head -n 1)
  created_at=$(is_outdated "$file")
  if [ -n "$created_at" ]; then
    >&2 echo "File '$file_name' in FOSS package '$package_name' for SHA $sha is outdated (created at $created_at)."
    return
  fi

  printf '%s' "$file" | jq ".id"
}

download_generic() {
  package_name="${1:?Expected package name}"
  sha="${2:?Expected SHA}"
  file_name="${3:?Expected file name}"

  file_id=$(get_generic_file_id "$package_name" "$sha" "$file_name")
  if [ -z "$file_id" ]; then
    >&2 echo "FOSS package file '$file_name' not available for '$package_name' at SHA $sha."
    return 1
  fi

  # Unfortunately there doesn't seem to be an API endpoint to retrieve (download) the file by ID.
  # But according to https://docs.gitlab.com/ee/user/packages/generic_packages/#download-package-file ,
  # "the most recent one is retrieved" when retrieving by name (as we do below), which should be
  # the one with the file_id that we determined.
  gitlab_api get "packages/generic/$package_name/$sha/$file_name" --output "$file_name"
  echo "Downloaded FOSS package file $file_id from packages/generic/$package_name/$sha/$file_name."
}

case $command in
  download-generic)
    download_generic "$@"
    ;;
  *)
    >&2 echo "Unsupported command: $command"
    exit 1
    ;;
esac
