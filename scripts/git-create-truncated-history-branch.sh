#!/usr/bin/env sh
# Creates a new branch that has the same content as the current commit/branch, but without any history.
# Aborts if the target branch already exists.
# Switches to the new branch automatically
#
# Example usage: creating a truncated branch from main
#
#   ```
#   > git checkout main
#   Switched to branch 'main'
#
#   > git-create-truncated-history-branch.sh truncated-main "truncated history of main"
#   ...
#
#   > git branch --show-curent
#   truncated-main
#
#   > git diff main | wc -l
#   0
#
#   > git rev-list main --count
#   19318
#
#   > git rev-list truncated-main --count
#   1
#   ```

set -eu

if [ "$#" -ne 2 ]; then
    >&2 echo "Wrong number of arguments."
    >&2 echo "Usage: $(basename "${0}") <truncated_branch_name> <commit_message>"
    exit 1
fi
readonly truncated_branch="${1}"
readonly commit_message="${2}"

if git show-ref --quiet refs/heads/"${1}"; then
  echo "Error: ${truncated_branch} already exists"
  exit 1
fi
git checkout --orphan "${truncated_branch}"
git add --all
git commit -m "${commit_message}"

