#!/usr/bin/env sh

# Download/delete/list packages from the GitLab package registry.

set -eu

SCRIPTSELF=$(command -v "$0")
readonly SCRIPTSELF
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
readonly SCRIPTPATH

# shellcheck source=scripts/sh-utils.sh
. "$SCRIPTPATH/sh-utils.sh"

usage() {
  echo "Usage: '$0' --git-dir <dir> --api-key <key> [--dry-run] <command> [args...]"
  echo "Commands:"
  echo "  download <package-type> <package-name> <sha> [<file-name>]"
  echo "  delete <package-type> <package-name> <version (the sha for generic packages)>"
  echo "  list <package-type> [<package-name> [<version>]]"
  echo "  npm-dist-tags [<package-name>]"
}

git_dir=''
api_key=''
while [ "$#" != 0 ]; do
  case "$1" in
    --git-dir)
      shift; git_dir="${1:?Expected value for --git-dir}" ;;
    --api-key)
      shift; api_key="${1:?Expected value for --api-key}" ;;
    --dry-run)  # Only print what would be deleted, without actually deleting
      export DRY_DELETE=yes ;;
    --help|-h)
      usage
      exit ;;
    --)
      shift
      break ;;
    --*)
      >&2 echo "$0: Unknown option: $1"
      >&2 usage
      exit 2 ;;
    *) break ;;
  esac
  shift
done

readonly git_dir="${git_dir:?Expected --git-dir}"
readonly api_key="${api_key:?Expected --api-key}"
readonly command="${1:?Expected command}"; shift

gitlab_api() {
  "$SCRIPTPATH"/gitlab-api.sh "$git_dir" "$api_key" "$@"
}

urlencode() {
  "$SCRIPTPATH"/url.sh encode "$1"
}

is_outdated() {
  raw_echo "$1" | "$SCRIPTPATH"/gitlab-api.sh "$git_dir" "$api_key" get-outdated-creation-timestamp
}

get_generic_file_id() {
  package_name="$1"
  sha="$2"
  file_name="$3"

  # https://docs.gitlab.com/api/packages/#for-a-project
  package=$(gitlab_api get "packages?package_name=$(urlencode "$package_name")&package_type=generic&package_version=$sha" | jq first)
  if [ -z "$package" ] || [ "$package" = "null" ]; then
    >&2 echo "FOSS package '$package_name' not found for SHA $sha."
    return
  fi

  package_id=$(raw_echo "$package" | jq ".id")
  # https://docs.gitlab.com/api/packages/#list-package-files
  package_files=$(gitlab_api get-multipage "packages/$package_id/package_files")
  files=$(raw_echo "$package_files" \
    | jq --arg file_name "$file_name" --compact-output '.[] | select( .file_name == $file_name ) | { created_at, id }' \
    | sort -r)

  if [ -z "$files" ]; then
    >&2 echo "No file named '$file_name' found in FOSS package '$package_name' for SHA $sha."
    return
  fi

  file=$(raw_echo "$files" | head -n 1)
  created_at=$(is_outdated "$file")
  if [ -n "$created_at" ]; then
    >&2 echo "File '$file_name' in FOSS package '$package_name' for SHA $sha is outdated (created at $created_at)."
    return
  fi

  raw_echo "$file" | jq ".id"
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
  # But according to (a previous version of the) documentation, "the most recent one is retrieved" when
  # retrieving by name (as we do below), which should be the one with the file_id that we determined.
  # https://docs.gitlab.com/user/packages/generic_packages/#download-a-single-file
  gitlab_api get "packages/generic/$package_name/$sha/$file_name" --output "$file_name"
  echo "Downloaded FOSS package file $file_id from packages/generic/$package_name/$sha/$file_name."
}

# Print the dist-tags of one npm package as json, or nothing if the package does not exist
get_package_npm_dist_tags() {
  package_name="${1:?Expected package name}"

  # https://docs.gitlab.com/api/packages/npm/#retrieve-package-metadata
  metadata=$(gitlab_api get "packages/npm/$package_name" 2>/dev/null) || true
  # A missing package yields an empty response, since curl --fail suppresses the error body
  if [ -z "$metadata" ]; then
    return
  fi
  # gitlab answers requests for unknown packages with a redirect notice pointing to registry.npmjs.org instead of a 404, also treat that as missing
  if raw_echo "$metadata" | jq --exit-status 'type == "string" and test("registry\\.npmjs\\.org")' >/dev/null 2>&1; then
    return
  fi
  dist_tags=$(raw_echo "$metadata" | jq --raw-input --slurp 'fromjson? | select(type == "object") | ."dist-tags" // {}')
  if [ -z "$dist_tags" ]; then
    fail "Invalid response for npm package '$package_name': $metadata"
  fi
  raw_echo_trailing_newline "$dist_tags"
}

get_all_npm_dist_tags() {
  npm_packages=$(list_packages npm)
  names=$(raw_echo "$npm_packages" | jq --raw-output '.[].name' | sort -u)
  all_dist_tags='{}'
  for name in $names; do
    dist_tags=$(get_package_npm_dist_tags "$name")
    if [ -n "$dist_tags" ]; then
      all_dist_tags=$(raw_echo "$all_dist_tags" \
        | jq --arg name "$name" --argjson dist_tags "$dist_tags" '. + { ($name): $dist_tags }')
    fi
  done
  raw_echo_trailing_newline "$all_dist_tags"
}

# Dist-tags of the given npm package, or of all npm packages if no package name is given
get_npm_dist_tags() {
  if [ -z "${1-}" ]; then
    get_all_npm_dist_tags
  else
    get_package_npm_dist_tags "$1"
  fi
}

get_npm_package_version() {
  package_name="$1"
  sha="$2"

  dist_tags=$(get_package_npm_dist_tags "$package_name")
  if [ -z "$dist_tags" ]; then
    >&2 echo "FOSS npm package '$package_name' not found for SHA $sha."
    return
  fi
  version=$(raw_echo "$dist_tags" | jq -r --arg sha "$sha" '.[$sha] // empty')
  if [ -z "$version" ]; then
    >&2 echo "FOSS npm package '$package_name' has no dist-tag for SHA $sha."
    return
  fi
  echo "$version"
}

download_npm() {
  package_name="${1:?Expected package name}"
  sha="${2:?Expected SHA}"

  version=$(get_npm_package_version "$package_name" "$sha")
  if [ -z "$version" ]; then
    >&2 echo "FOSS npm package '$package_name' not available for SHA $sha."
    return 1
  fi

  file_name="${package_name}-${version}.tgz"
  # https://docs.gitlab.com/api/packages/npm/#download-a-package
  gitlab_api get "packages/npm/$package_name/-/$file_name" --output "${package_name}.tgz"
  echo "Downloaded FOSS npm package '$package_name@$version'."
}

download_package() {
  package_type="${1:?Expected package type}"; shift
  case $package_type in
    generic)
      download_generic "$@" ;;
    npm)
      download_npm "$@" ;;
    *)
      >&2 echo "Unsupported package type: $package_type"
      exit 1 ;;
  esac
}

# Print a JSON array of package version objects, optionally filtered by exact name and version
# https://docs.gitlab.com/ee/api/packages.html#for-a-project
list_packages() {
  package_type="${1:?Expected package type}"
  package_name="${2-}"
  version="${3-}"

  path="packages?package_type=$package_type"
  if [ -n "$package_name" ]; then
    # The package_name filter is fuzzy; we select on the exact name below
    path="$path&package_name=$(urlencode "$package_name")"
  fi
  if [ -n "$version" ]; then
    path="$path&package_version=$version"
  fi

  packages_json=$(gitlab_api get-multipage "$path")
  # Also filter locally, in case e.g. the package_version filter is not supported
  raw_echo "$packages_json" \
    | jq --arg name "$package_name" --arg version "$version" \
        '[ .[] | select( ($name == "" or .name == $name) and ($version == "" or .version == $version) ) ]'
}

delete_package() {
  package_type="${1:?Expected package type}"
  package_name="${2:?Expected package name}"
  version="${3:?Expected version}"

  # Besides regular packages this also lists (and thus deletes) error-status
  # leftovers of rejected duplicate publishes.
  list_packages "$package_type" "$package_name" "$version" \
    | jq --compact-output '.[]' \
    | while read -r package; do
        package_id=$(raw_echo "$package" | jq ".id")
        >&2 echo "${DRY_DELETE:+(dry run) }Deleting $package_type package '$package_name@$version' (id $package_id, status $(raw_echo "$package" | jq -r ".status"))."
        # https://docs.gitlab.com/api/packages/#delete-a-project-package
        gitlab_api delete "packages/$package_id"
      done
}

case $command in
  download)
    download_package "$@"
    ;;
  delete)
    delete_package "$@"
    ;;
  list)
    list_packages "$@"
    ;;
  npm-dist-tags)
    get_npm_dist_tags "$@"
    ;;
  *)
    >&2 echo "Unsupported command: $command"
    >&2 usage
    exit 2
    ;;
esac
