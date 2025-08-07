#!/usr/bin/env sh

# Checks whether "include"d CI YML files have the correct version.

# PEP project repositories (typically) have (a) submodule(s) for the PEP source
# repository (and optionally other PEP repositories). The PEP source repository
# a.o. provides prefab CI logic in its "ci_cd/pep-project-ci-*.yml" files.
# (Customer) project repositories use Gitlab's "include" feature to use the
# prefab CI.
# We'd like to include the .yml file from the submodule (directory), so that
# - the project repository's submodule version determines the version of the
#   included CI logic, and
# - any CI support files (such as scripts in the PEP source repo's "ci_cd"
#   directory) would have the same version as the included .yml file.
# But Gitlab does not support including CI .yml files from submodules (see e.g.
# https://gitlab.com/gitlab-org/gitlab/-/issues/25249). Instead it retrieves
# (a version of) the included .yml directly from the other repository. It is
# therefore possible that the project repo's includes prefab CI that is based
# on other "ci_cd" script versions than the sumodule provides. This could
# introduce hard-to-diagnose problems in CI pipelines.
# To prevent such problems, we ensure that included CI .yml files are identical
# to the corresponding files in the submodule. Then, if a problem _does_ pop up
# in a CI pipeline, the pipeline will have been based on the file versions of
# the pipeline's commit, instead of whatever file version was in the upstream
# repository at the time that the pipeline ran.

set -o errexit
set -o nounset

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"

api_key="$1"
git_dir="${2:-.}"
git_root=$(cd "$git_dir" && pwd)
ci_yml="${3:-$git_root/.gitlab-ci.yml}"

get_gitdir_host() {
  dir="$1"
  "$SCRIPTPATH"/../scripts/gitdir.sh origin-host "$dir"
}

evaluate_ref() {
  ref="$1"

  # Deal with empty parameter (degenerate case)
  if [ -z "$ref" ]; then
    >&2 echo "No ref to evaluate"
    exit 1
  fi

  # Check if ref is variable, e.g. $CI_COMMIT_REF_NAME
  firstchar=$(echo "$ref" | cut -c1)
  if [ "$firstchar" != "\$" ] ; then
    echo "$ref"
  else
    # Get specified variable (name)
    refvar=$(echo "$ref" | cut -c2-)
    if [ -z "$refvar" ]; then
      >&2 echo "Ref cannot be a single \$ symbol"
      exit 1
    fi

    # Strip curly braces from variable "ref" name, e.g. ${CI_COMMIT_REF_NAME}
    firstchar=$(echo "$refvar" | cut -c1)
    lastchar=$(echo "$refvar" | cut -c"${#refvar}")
    if [ "$firstchar" = "{" ] && [ "$lastchar" = "}" ]; then
      lastpos="$((${#refvar}-1))"
      refvar=$(echo "$refvar" | cut -c2-"$lastpos")
    fi

    # Evaluate the variable
    eval echo \$"$refvar"
  fi
}

check_version_if_included() {
  submod_dir="$1"
  file="$2"

  if [ "$(get_gitdir_host "$git_root")" = "$(get_gitdir_host "$submod_dir")" ] ; then
    submod_path=$("$SCRIPTPATH"/../scripts/gitdir.sh origin-path "$submod_dir")
    ref=$("$SCRIPTPATH"/get-gitlab-ci-yml-include-ref.py "$ci_yml" "$submod_path" "/$file")

    if [ -n "$ref" ]; then
      ref=$(evaluate_ref "$ref")
      >&2 echo CI includes "$ref" version of "$file"
      url="repository/files/$("$SCRIPTPATH"/../scripts/url.sh encode "$file")/raw"
      >&2 echo "Retrieving $url with ref=$ref using Gitlab API"
      "$SCRIPTPATH"/../scripts/gitlab-api.sh "$submod_dir" "$api_key" get "$url" --data-urlencode "ref=$ref" > included.yml
      diffs=$(diff "$submod_dir/$file" included.yml) && diffresult=$? || diffresult=$?
      if [ "$diffresult" != 0 ]; then
        >&2 echo \
          "Your CI includes the $ref version of file $file from the $submod_path repository," \
          "but the submodule directory at $submod_path has a different version."
        >&2 echo \
          "This (possibly) makes the YML's CI definition inconsistent/incompatible with other" \
          "CI support files in the submodule. To fix, either"
        >&2 echo "- update your submodule to the $ref version, or"
        >&2 echo "- specify an appropriate 'ref' with the 'include' in your $ci_yml."
        >&2 echo "See e.g. https://gitlab.com/gitlab-org/gitlab/-/issues/25249 ."
        >&2 echo
        >&2 echo "Diffs between the submodule (left) and included (right) versions are:"
        >&2 echo "$diffs"
        exit "$diffresult"
      else
        echo The included "$ref" version of file "$file" from the "$submod_path" project is equal to the version in the submodule directory at "$submod_dir": CI may proceed.
      fi
    fi
  fi
}

# For every git submodule directory...
# TODO: also check own directory: we may include YML from our own repository
"$SCRIPTPATH"/../scripts/gitdir.sh submodule-dirs "$git_root" | while read -r submod_dir
do
  # ...combined with every prefab CI file (path) that we provide...
  for file in ci_cd/pep-project-ci-settings.yml ci_cd/pep-project-ci-logic.yml
  do
    # ...if the CI logic includes the file from that submodule, ensure it has the correct version.
    check_version_if_included "$submod_dir" "$file"
  done
done
