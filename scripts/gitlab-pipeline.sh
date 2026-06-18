#!/usr/bin/env sh

# Manages GitLab CI pipelines.

set -eu

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

# shellcheck source=scripts/sh-utils.sh
. "$SCRIPTPATH/sh-utils.sh"

git_dir="${1:?Expected git dir}"; shift
api_key="${1:?Expected API key}"; shift
command="${1:?Expected command}"; shift

gitlab_api() {
  "$SCRIPTPATH"/gitlab-api.sh "$git_dir" "$api_key" "$@"
}

get_pipeline_id() {
  branchname="$1"
  max_retries=5
  retry_delay=5
  attempt=1

  while [ $attempt -le $max_retries ]; do
    sleep $retry_delay

    id=$(gitlab_api get "pipelines" | jq -r "[.[] | select(.ref==\"$branchname\")] | .[0].id")
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

trigger_and_wait() {
  branchname="${1:?Expected branch name}"
  ref_sha="${2:?Expected ref SHA}"
  pipeline_name="${3:-}"

  gitlab_host=$("$SCRIPTPATH"/gitdir.sh origin-host "$git_dir")
  project_path=$("$SCRIPTPATH"/gitdir.sh origin-path "$git_dir")

  # GitLab can only run pipelines for branches (or tags): see https://stackoverflow.com/a/63460457
  gitlab_api post "repository/branches?branch=$branchname&ref=$ref_sha" > /dev/null

  pipeline_id=$(get_pipeline_id "$branchname") || return 1

  if [ -n "$pipeline_name" ]; then
    gitlab_api put "pipelines/$pipeline_id/metadata" --data "name=$pipeline_name" > /dev/null || true
  fi

  echo "Running pipeline $pipeline_id in project $project_path for branch $branchname:" \
    "https://$gitlab_host/$project_path/-/pipelines/$pipeline_id"

  running_statuses=$(printf '%s\n' pending running created preparing waiting_for_resource)
  success_statuses=$(printf '%s\n' success skipped manual)
  failure_statuses=$(printf '%s\n' failed canceled canceling)

  pipeline_result=
  while [ -z "$pipeline_result" ]; do
    status=$(gitlab_api get "pipelines/${pipeline_id}" | jq -r ".status")

    if list_contains "$success_statuses" "$status"; then
      echo "Pipeline succeeded with status: '$status'"
      pipeline_result=0
    elif list_contains "$failure_statuses" "$status"; then
      >&2 echo "Pipeline failed with status: '$status'"
      pipeline_result=1
    elif ! list_contains "$running_statuses" "$status"; then
      >&2 echo "Received unsupported status \"$status\" from GitLab API"
      pipeline_result=1
    fi

    sleep 30
  done

  gitlab_api delete "repository/branches/$branchname"

  return $pipeline_result
}

case $command in
  trigger-and-wait)
    trigger_and_wait "$@"
    ;;
  *)
    >&2 echo "Unsupported command: $command"
    exit 1
    ;;
esac
