#!/usr/bin/env sh

set -o errexit
set -o nounset

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

die() {
  exit 1
}

invalid_invocation() {
  >&2 echo "Usage: $0 <infra-dir> <project-dir> [pipeline-id]"
  die
}

if [ "$#" -lt 2 ]; then
  >&2 echo "Too few command line parameters."
  invalid_invocation
fi

if [ "$#" -gt 4 ]; then
  >&2 echo "Too many command line parameters."
  invalid_invocation
fi

infraDir="$1"
projectDir="$2"
pipelineId="${3:-${CI_PIPELINE_ID:-0}}"
jobId="${4:-${CI_JOB_ID:-0}}"

projectPath=$("$SCRIPTPATH"/gitdir.sh origin-path "$infraDir")
projectRoot=$("$SCRIPTPATH"/gitdir.sh get-project-root "$infraDir")
projectCaption=$(cat "$projectDir"/caption.txt)

majorVersion="$($SCRIPTPATH/parse-version.sh get-major "$projectRoot/version.json")"
minorVersion="$($SCRIPTPATH/parse-version.sh get-minor "$projectRoot/version.json")"

# Use branch/tag name for CI builds, e.g. using feature branch names instead of "review".
# Use infra directory name for local builds so that the environment has a readable caption.
reference="${CI_COMMIT_REF_NAME:-$(basename "$infraDir")}"

# Use an empty revision if commit SHA cannot be determined, e.g. when the
# project directory is not stored in a git repository.
# See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2193
revision=$("$SCRIPTPATH"/gitdir.sh commit-sha "$projectDir" || true)
if [ -n "${CI_COMMIT_SHA+x}" ]; then # Check if variable is set: https://stackoverflow.com/a/13864829
  if [ "$revision" != "$CI_COMMIT_SHA" ]; then
    >&2 echo "The project directory's commit SHA '$revision' does not match the CI_COMMIT_SHA '$CI_COMMIT_SHA'"
    die
  fi
fi

if [ -n "${CI_PIPELINE_ID+x}" ]; then # Check if variable is set: https://stackoverflow.com/a/13864829
  if [ "$pipelineId" != "$CI_PIPELINE_ID" ]; then
    >&2 echo "The specified pipeline ID '$pipelineId' does not match the CI_PIPELINE_ID '$CI_PIPELINE_ID'"
    die
  fi
fi



# TODO: include details on both infra and project directory?
echo "{"
echo "  \"projectPath\"   : \"$projectPath\","
echo "  \"projectCaption\": \"$projectCaption\","
echo "  \"reference\"     : \"$reference\","
echo "  \"revision\"      : \"$revision\","
echo "  \"majorVersion\"  : $majorVersion,"
echo "  \"minorVersion\"  : $minorVersion,"
echo "  \"pipelineId\"    : $pipelineId,"
echo "  \"jobId\"         : $jobId"
echo "}"
