#!/usr/bin/env sh
# shellcheck disable=SC3043  # `local` may not be defined in POSIX sh

set_opts() {
  set -eux -o noglob
}
set_opts

scriptdir="$(realpath "$(dirname -- "$0")")"

fail() {
  >&2 echo "$@"
  exit 1
}

# `&& true` prevents quitting for nonzero exit code
getopt --test >/dev/null && true
[ "$?" -ne 4 ] && fail 'Enhanced getopt(1) required'

args="$(getopt \
  --options='h' \
  --longoptions='help,api-key:,no-dry-run,dtap-dir:,unshallow,delete-single' \
  --name "$0" -- "$@")"
eval set -- "$args"

no_dry_run=''
should_unshallow=''
should_delete_single=''
while true; do
  case "$1" in
    --api-key)
      api_key="$2"
      shift ;;
    --no-dry-run)  # Do not perform a dry run (default is dry run)
      no_dry_run=yes ;;
    --dtap-dir)
      dtap_dir="$2"
      shift ;;
    --unshallow)  # Unshallow git repositories when necessary (otherwise error)
      should_unshallow=yes ;;
    --delete-single)  # Delete only the first object per cleanup (for testing)
      should_delete_single=yes ;;
    --help|-h)
      echo "Usage: '$0' --api-key=... [--no-dry-run] [--dtap-dir=...] [--unshallow] <command, e.g. clean-all>"
      exit ;;
    --)
      shift
      break ;;
    *) fail "Unimplemented option: $1"
  esac
  shift
done

command="${1:?Expected command}"

if [ "$#" -gt 1 ]; then
  >&2 echo "$0: Too many arguments"
  exit 2
fi

foss_package_names=\
'wixlibrary
macos-x86-bins
macos-arm-bins
flatpak
'
foss_container_image_names=\
'pep-monitoring
authserver_apache
pep-services
client
'
dtap_container_image_names="$foss_container_image_names
backup-tool
nginx
prometheus
testidp
logger
nginx-review
docker-compose
watchdog-watchdog
"

# Also keep docker-build images for n commits in FOSS before the latest commit.
# This is a bit of a hack to make sure that projects not on the very last commit can still build
keep_extra_docker_build=20

foss_docker_build_submodule='docker-build'
dtap_foss_submodule='pep'

foss_dir="$scriptdir/../"
docker_build_dir="$foss_dir$foss_docker_build_submodule/"
if [ -n "${dtap_dir+defined}" ]; then
  dtap_dir="$(realpath "$dtap_dir")"
fi

chr_tab="$(printf '\t')"
# A cross-platform sh-compatible way to put just a newline into a variable (`$(...)` trips trailing newlines)
chr_lf="$(printf '\nX')"
chr_lf="${chr_lf%X}"
chr_crlf="$(printf '\r\nX')"
chr_crlf="${chr_crlf%X}"

# Echo without newlines or processed escapes (which some sh implementations do)
raw_echo() {
  printf %s "$*"
}

# Ensure nonempty line ends with a newline
raw_echo_trailing_newline() {
  local str="$*"
  raw_echo "$str"
  # Nonempty & does not end with newline?
  if [ -n "$str" ] && [ "${str%"$chr_lf"}" = "$str" ]; then
    echo
  fi
}

# Forwards 0/1 exit code, exits for other exit codes
# Usage: `if command_returning_0_1 || boolean; then ...; fi`, invert with `! boolean`
boolean() {
  case "$?" in
    0|1) return "$?" ;;
    *) exit "$?" ;;
  esac
}

# Check if a list where all lines end in \n contains a line
list_contains() {
  local list="$1"
  local needle="$2"

  # == grep --quiet --line-regexp --fixed-strings (long form not supported on BusyBox)
  raw_echo "$list" | grep -qxF "$needle" || boolean
}

gitlab_dir_api() {
  local dir="$1"
  shift
  "$scriptdir/gitlab-api.sh" "$dir" "${api_key:?Pass --api-key}" "$@"
}

# 1. Make sure that we are not in a shallow repository,
#    because we want to see all remote branches.
# 2. Fetch remote.
# 3.  Make sure we have origin/HEAD pointing to the main branch
prepare_git_repo() {
  if [ "$(git rev-parse --is-shallow-repository)" != false ]; then
    if [ -n "$should_unshallow" ]; then
      # Convert shallow clone into regular one
      >&2 echo "Unshallowing shallow repository $PWD"
      git remote set-branches origin '*' >&2
      set +e
      fetch_err="$(git fetch --unshallow 2>&1)"
      fetch_status=$?
      set -e
      echo "git fetch --unshallow exited with status $fetch_status" >&2
      if [ $fetch_status -ne 0 ]; then
        fail "git fetch --unshallow failed: $fetch_err"
      fi
    else
      fail "$PWD is a shallow repository, pass --unshallow to unshallow"
    fi
  else
    git fetch >&2
  fi

  git remote set-head origin --auto >&2  # Make sure we have origin/HEAD pointing to the main branch
}

# Convert special characters in name just like GitLab does for e.g. $CI_COMMIT_REF_SLUG.
# Based on slugify from GitLab https://gitlab.com/gitlab-org/gitlab/-/blob/9e379cc4edba7fbe4777b6b7267c43eb81cd04cd/gems/gitlab-utils/lib/gitlab/utils.rb#L56-67
slugify() {
  local name="$1"
  raw_echo "$name" |
    tr '[:upper:]' '[:lower:]' |
    tr -c '[:alnum:]' '-' |  # == tr --complement
    cut -c '-63' |  # == cut --characters=-63
    sed 's/^-*//;s/-*$//'
}

confirm_deletion() {
  local what_description="${1-}"

  [ -z "$no_dry_run" ] && fail "Dry run shouldn't arrive at this point"
  [ -n "$should_delete_single" ] && echo 'Actually, only the first of these will be deleted because of --delete-single'
  # sh approximation of isatty
  if [ -c /dev/stdin ]; then
    printf 'Delete these %s (y/n)? ' "$what_description"
    local choice
    read -r choice
    case "$choice" in
      [Yy]*) ;;
      *)
        >&2 echo Aborting
        exit 1 ;;
    esac
  fi
}

# Template cleanup function that does the following:
# 1. List used versions
# 2. List versions that are eligible for cleanup
# 3. Subtract (1) from (2) to obtain obsolete versions
# 4. Ask for confirmation
# 5. Delete obsolete versions
generic_cleanup() {
  local cleanup_description="$1"   # Long description of the cleanup operation, e.g. 'docker-build container cleanup'
  local what_description="$2"      # Description of what kind of thing we are throwing away, e.g. 'package versions'
  local list_used_fun="$3"         # Function returning whitespace-separated version names in use
  local list_existing_fun="$4"     # Function returning stream of JSON objects for existing versions that could be deleted
  local json_select_version="$5"   # `jq` selector to get version name from existing versions, e.g. '.version'
  local json_display_version="$6"  # `jq` expression to display existing version
  local delete_version_fun="$7"    # Function to delete version, taking version JSON object as parameter

  printf '\n\n==== Initiating %s ====\n' "$cleanup_description"

  # ==== Determine used versions ====
  >&2 printf '\nListing used %s...\n\n' "$what_description"
  local used_versions
  used_versions="$(set_opts && $list_used_fun)"
  [ -z "$used_versions" ] && fail "Didn't find any $what_description to keep, something is wrong"
  >&2 echo "Found $(raw_echo "$used_versions" | wc -w) $what_description that may be in use"
  >&2 echo "$used_versions"

  # ==== List existing versions ====
  >&2 printf '\nListing existing %s...\n\n' "$what_description"
  local existing_versions existing_len
  existing_versions="$(set_opts && $list_existing_fun)"
  existing_len="$(raw_echo "$existing_versions" | jq --slurp length)"
  >&2 echo "Found $existing_len $what_description"

  # ==== Determine obsolete versions ====
  # Select not contained within $used_versions
  local obsolete_versions
  obsolete_versions="$(
    # Select all versions where #{name}\obsolete_names == 1, so name is not in obsolete_names
    # shellcheck disable=SC2086 # Split $used_versions
    raw_echo "$existing_versions" | jq --compact-output 'select(['"$json_select_version"'] - $ARGS.positional | length == 1)' --args $used_versions
  )"
  local obsolete_len
  obsolete_len="$(raw_echo "$obsolete_versions" | jq --slurp length)"
  echo "Found $obsolete_len obsolete $what_description"
  raw_echo "$obsolete_versions" | jq --raw-output "$json_display_version"

  >&2 echo "$obsolete_len/$existing_len $what_description will be deleted"
  [ "$obsolete_len" = "$existing_len" ] && fail "Refusing to delete all $what_description"

  # ==== Confirm ====
  [ -z "$no_dry_run" ] && return
  printf '\n=> %s\n' "$cleanup_description"
  confirm_deletion "$what_description"

  # ==== Delete versions ====
  >&2 printf '\nDeleting %s for %s...\n\n' "$what_description" "$cleanup_description"
  # We used --compact-output for $obsolete_versions, so objects are on their own line
  raw_echo_trailing_newline "$obsolete_versions" | while read -r ver_obj; do
    >&2 echo "Deleting $(raw_echo "$ver_obj" | jq --raw-output "$json_display_version")"
    $delete_version_fun "$ver_obj"
    if [ -n "$should_delete_single" ]; then break; fi
  done

  printf '\n%s complete.\n\n' "$cleanup_description"
}

# Get lines with short protected branch names, may contain wildcards
list_protected_branch_names() {
  local dir="$1"

  # List protected branches using GitLab API
  # https://docs.gitlab.com/ee/api/protected_branches.html#list-protected-branches
  local protected_branches_raw protected_branch_names
  protected_branches_raw="$(gitlab_dir_api "$dir" get-multipage protected_branches)"
  protected_branch_names="$(raw_echo "$protected_branches_raw" | jq --raw-output '.[].name')"
  [ -z "$protected_branch_names" ] && fail 'No protected branches found?'
  raw_echo "$protected_branch_names"
}

# Get lines with unique commit SHAs from tips of protected branches/tags
list_protected_commits() {
  local dir="$1"

  # List protected branches & tags using GitLab API

  local protected_branch_names
  protected_branch_names="$(set_opts && list_protected_branch_names "$dir")"

  # I'd like to use https://docs.gitlab.com/ee/api/protected_tags.html#list-protected-tags,
  # but it gives 403 for our Developer API token, see https://gitlab.com/gitlab-org/gitlab/-/issues/470974
  # https://docs.gitlab.com/ee/api/tags.html#list-project-repository-tags
  local tags_raw protected_tag_names
  tags_raw="$(gitlab_dir_api "$dir" get-multipage repository/tags)"
  protected_tag_names="$(raw_echo "$tags_raw" | jq --raw-output '.[] | select(.protected).name')"

  local commits
  commits="$(set_opts
    cd "$dir"
    prepare_git_repo

    IFS="$chr_crlf"  # Split arguments to `git for-each-ref` on newlines
    # Lookup commits at the tip of each protected branch & tag, interpreting wildcards like release-*.*
    # shellcheck disable=SC2046 # We explicitly specify IFS
    git for-each-ref \
      $(raw_echo "$protected_branch_names" | sed 's/^/refs\/remotes\/origin\//') \
      $(raw_echo "$protected_tag_names" | sed 's/^/refs\/tags\//') |
      while IFS=" $chr_tab" read -r objectname objecttype refname; do
        >&2 echo "$objectname used by protected $objecttype $refname"
        echo "$objectname"
      done
  )"
  # Split from assignment because we don't have pipefail
  raw_echo "$commits" | sort | uniq
}

# List all relevant image tag objects in a project.
# Returns: lines with tag objects (https://docs.gitlab.com/ee/api/container_registry.html#within-a-project-1) + repository_id
list_project_image_tags() {
  local dir="$1"
  local list_image_names="$2"      # Whitespace-separated list of image names to include
  local json_tag_filter="${3:-.}"  # `jq` filter for tags to include, e.g. 'select(.name | startswith("sha-"))'

  # List all project container images
  local all_image_repos
  all_image_repos="$(list_project_container_repositories "$dir")"

  local img_name
  for img_name in $list_image_names; do
    >&2 echo "Listing tags for container repository $img_name"

    local repo_id
    repo_id="$(raw_echo "$all_image_repos" | jq '.[] | select(.name == $img_name).id' --arg img_name "$img_name")"
    [ -z "$repo_id" ] && fail "Container repo for $img_name not found"

    local tags
    tags="$(list_project_container_repository_tags "$dir" "$repo_id")"
    # Split from assignment because we don't have pipefail
    # Add repository ID for later
    # Filter tags
    raw_echo "$tags" | jq --compact-output '.[] + {"repository_id": $repo_id} | '"$json_tag_filter"'' --arg repo_id "$repo_id"
  done
}


# ==== Common container image tag cleanup code ====

# https://docs.gitlab.com/ee/api/container_registry.html#within-a-project
list_project_container_repositories() {
  local dir="$1"
  gitlab_dir_api "$dir" get-multipage registry/repositories
}
# https://docs.gitlab.com/ee/api/container_registry.html#within-a-project-1
list_project_container_repository_tags() {
  local dir="$1"
  local repo_id="$2"
  gitlab_dir_api "$dir" get-multipage "registry/repositories/$repo_id/tags"
}

delete_container_image_tag() {
  local dir="$1"
  local tag_obj="$2"  # Tag object + .repository_id

  local image_repo image_tag
  image_repo="$(raw_echo "$tag_obj" | jq --raw-output .repository_id)"
  image_tag="$(raw_echo "$tag_obj" | jq --raw-output .name)"
  # https://docs.gitlab.com/ee/api/container_registry.html#delete-a-registry-repository-tag
  gitlab_dir_api "$dir" delete "registry/repositories/$image_repo/tags/$image_tag" >&2
}

delete_container_repository() {
  local dir="$1"
  local repo_obj="$2"  # Repository object

  local image_repo
  image_repo="$(raw_echo "$repo_obj" | jq --raw-output .id)"
  # https://docs.gitlab.com/ee/api/container_registry.html#delete-registry-repository
  gitlab_dir_api "$dir" delete "registry/repositories/$image_repo" >&2
}

# ==== Common package version cleanup code ====

# https://docs.gitlab.com/ee/api/packages.html#for-a-project
list_project_package_versions() {
  local dir="$1"
  local name="$2"
  gitlab_dir_api "$dir" get-multipage 'packages?package_type=generic' --data-urlencode "package_name=$name"
}

delete_package_version() {
  local dir="$1"
  local ver_obj="$2"  # Package version object

  local id
  id="$(raw_echo "$ver_obj" | jq --raw-output .id)"
  # https://docs.gitlab.com/ee/api/container_registry.html#delete-a-registry-repository-tag
  gitlab_dir_api "$dir" delete "packages/$id" >&2
}


# ===================================================
# ==== docker-build container image tags cleanup ====
# ===================================================
# See https://gitlab.pep.cs.ru.nl/pep/docker-build/container_registry.
# Image tags that we clean up are `sha-<commit SHA>`.
# Tags for commits on the tip of a branch / git tag will be kept,
# just like tags for commits used on the tip of a branch (or up to $keep_extra_docker_build commits before that) / git tag in PEP FOSS.

# List docker-build image tags that we want to keep:
# tags that are used in some git tag or the tip of some branch in PEP FOSS or docker-build itself.
# Returns: lines with unique tag names
list_used_docker_build_image_tags() {
  local used_tags
  used_tags="$(set_opts
    # List docker-build image tags used by PEP FOSS repo
    (
      cd "$foss_dir"
      prepare_git_repo
      git for-each-ref | while read -r objectname objecttype refname; do
        case "$objecttype" in
          commit|tag)
            ancestors="$(set_opts
              if [ "$objecttype" = commit ]; then
                git rev-list "$objectname" --max-count="$((keep_extra_docker_build+1))"
              else
                echo "$objectname"
              fi
            )"
            for ancestor in $ancestors; do
              # Could also parse ci_cd/docker-common.yml (via git show) but this is probably easier.
              # We leave out --verify to handle refs where docker-build doesn't exist
              docker_build_commit="$(git rev-parse --revs-only "$ancestor:$foss_docker_build_submodule")"
              if [ -n "$docker_build_commit" ]; then
                >&2 echo "docker-build $docker_build_commit needed by $objecttype $refname (commit $ancestor)"
                echo "sha-$docker_build_commit"
              fi
            done
            ;;
        esac
      done
    )

    # List docker-build tags from commits at the tip of a branch / tag in docker-build
    (
      cd "$docker_build_dir"
      prepare_git_repo
      git for-each-ref | while read -r objectname objecttype refname; do
        case "$objecttype" in
          commit|tag)
            >&2 echo "docker-build $objectname pointed to by docker-build $objecttype $refname"
            echo "sha-$objectname"
            ;;
        esac
      done
    )
  )"
  # Split from assignment because we don't have pipefail
  raw_echo "$used_tags" | sort | uniq
}

# List all relevant image tag objects in docker-build
list_docker_build_image_tags() {
  # Which images do we want to clean up?
  local docker_build_container_image_names
  docker_build_container_image_names="$(yq .variables.DEPLOY_IMAGES "$docker_build_dir/.gitlab-ci.yml")"

  # Select sha-* tags
  list_project_image_tags "$docker_build_dir" \
    "$docker_build_container_image_names" \
    'select(.name | startswith("sha-"))'
}

delete_docker_build_image_tag() {
  local tag_obj="$1"
  delete_container_image_tag "$docker_build_dir" "$tag_obj"
}

# Command to clean docker-build unused container image tags in docker-build
clean_docker_build_containers() {
  generic_cleanup 'docker-build container cleanup' 'container image tags' \
    list_used_docker_build_image_tags \
    list_docker_build_image_tags \
    .name .path delete_docker_build_image_tag
}


# ===================================================
# ==== PEP FOSS generic package versions cleanup ====
# ===================================================
# See https://gitlab.pep.cs.ru.nl/pep/core/-/packages.
# Versions are just named `<commit SHA>`.
# We keep packages for protected references,
# but in theory all missing versions can be rebuilt automatically

# List PEP FOSS package versions that we may want to keep:
# versions for protected branches and tags in PEP FOSS
list_used_foss_package_versions() {
  list_protected_commits "$foss_dir"
}

# List all relevant package version objects in PEP FOSS.
# Returns: lines with version objects (https://docs.gitlab.com/ee/api/packages.html#for-a-project)
list_foss_package_versions() {
  local pkg_name
  for pkg_name in $foss_package_names; do
    >&2 echo "Listing versions for package $pkg_name"
    local versions
    versions="$(set_opts && list_project_package_versions "$foss_dir" "$pkg_name")"
    # Split from assignment because we don't have pipefail
    raw_echo "$versions" | jq --compact-output '.[]'
  done
}

delete_foss_package_version() {
  local ver_obj="$1"
  delete_package_version "$foss_dir" "$1"
}

# Command to clean PEP FOSS unused package versions
clean_foss_packages() {
  generic_cleanup 'PEP FOSS package cleanup' 'package versions' \
    list_used_foss_package_versions \
    list_foss_package_versions \
    .version \
    "\"\(.name)$chr_tab\(.version)$chr_tab(\(._links.web_path))\"" \
    delete_foss_package_version
}


# ===============================================
# ==== PEP FOSS container image tags cleanup ====
# ===============================================
# See https://gitlab.pep.cs.ru.nl/pep/core/container_registry.
# tags are just named `<commit SHA>`.
# We keep tags for protected references,
# but in theory all missing images can be rebuilt automatically

# List PEP FOSS image tags that we may want to keep:
# image tags for protected branches and git tags in PEP FOSS
list_used_foss_image_tags() {
  list_protected_commits "$foss_dir"
}

# List all relevant image tag objects in PEP FOSS
list_foss_image_tags() {
  # Select tags with names in SHA-1 commit format
  list_project_image_tags "$foss_dir" \
    "$foss_container_image_names" \
    'select(.name | test("^[0-9a-f]{40}$"))'
}

delete_foss_image_tag() {
  local tag_obj="$1"
  delete_container_image_tag "$foss_dir" "$tag_obj"
}

# Command to clean PEP FOSS unused container image tags
clean_foss_containers() {
  generic_cleanup 'PEP FOSS container cleanup' 'container image tags' \
    list_used_foss_image_tags \
    list_foss_image_tags \
    .name .path delete_foss_image_tag
}


# ===========================================
# ==== DTAP container repository cleanup ====
# ===========================================
# See https://gitlab.pep.cs.ru.nl/pep/ops/container_registry.
# There are separate image repositories per branch / git tag, prefixed with `branch/`.
# We can throw away a whole repository if the corresponding branch / git tag does not exist anymore.

# List branches & git tags in DTAP git repo
list_used_dtap_refs() {
  (
    # List DTAP branches/tags
    cd "${dtap_dir:?Pass --dtap-dir}"
    prepare_git_repo
    git for-each-ref --format='%(objecttype) %(refname) %(refname:lstrip=-1)' 'refs/remotes/origin/*' |
    while read -r objecttype refname refname_short; do
      case "$objecttype" in
        commit|tag)
          ref_slug="$(slugify "$refname_short")"
          >&2 echo "$ref_slug/* in used by $objecttype $refname"
          for img_name in $dtap_container_image_names; do
            raw_echo "$ref_slug/$img_name "
          done
          echo
          ;;
      esac
    done
  )
}

# List DTAP container repositories that have the format `*/<dtap_image>`.
# Returns: lines with repository objects (https://docs.gitlab.com/ee/api/container_registry.html#within-a-project)
list_dtap_container_repositories() {
  # List all project container images
  local all_image_repos
  all_image_repos="$(list_project_container_repositories "$dtap_dir")"
  local img_name
  for img_name in $dtap_container_image_names; do
    # Select repos with names ending in /img_name
    local repos
    repos="$(raw_echo "$all_image_repos" | jq --compact-output '.[] | select(.name | endswith("/\($img_name)"))' --arg img_name "$img_name")"
    [ -z "$repos" ] && fail "No container repos found for $img_name"
    raw_echo_trailing_newline "$repos"
  done
}

delete_dtap_container_repository() {
  local repo_obj="$1"
  delete_container_repository "$dtap_dir" "$repo_obj"
}

# Command to clean DTAP unused container repositories
clean_dtap_container_repositories() {
  generic_cleanup 'DTAP container repository cleanup' 'container repositories' \
    list_used_dtap_refs \
    list_dtap_container_repositories \
    .name .location delete_dtap_container_repository
}


# =================================
# ==== DTAP git branch cleanup ====
# =================================
# See https://gitlab.pep.cs.ru.nl/pep/ops/-/branches/all.
# The update-foss-submodule-in-dtap job in the PEP FOSS CI creates a branch in DTAP for each FOSS branch,
# where it sets the FOSS submodule to the pipeline commit.
# This command deletes branches in DTAP that:
# - Are not protected or the main branch
# - Have no corresponding branch in PEP FOSS with the same name
# - Have commit(s) since master that only update the FOSS submodule
# - Have a FOSS submodule at the tip of which the commit is also in FOSS master

# Command to clean DTAP git branches
clean_dtap_branches() {
  local cleanup_description='DTAP git branch cleanup'
  printf '\n\n==== Initiating %s ====\n' "$cleanup_description"

  (
    cd "${dtap_dir:?Pass --dtap-dir}"
    prepare_git_repo
  )

  local protected_branches
  protected_branches="$(set_opts
    protected_branch_names="$(set_opts && list_protected_branch_names "$dtap_dir")"

    cd "$dtap_dir"
    IFS="$chr_crlf"  # Split arguments to `git for-each-ref` on newlines
    # Interpret wildcards
    # shellcheck disable=SC2046 # We explicitly specify IFS
    git for-each-ref --format='%(refname)' \
      $(raw_echo "$protected_branch_names" | sed 's/^/refs\/remotes\/origin\//')
  )"

  # Short branch names in FOSS
  local foss_branch_names
  foss_branch_names="$(set_opts
    cd "$foss_dir"
    prepare_git_repo
    git for-each-ref --format='%(refname:lstrip=-1)' 'refs/remotes/origin/*'
  )"

  # Short obsolete branch names in DTAP
  local obsolete_branches
  obsolete_branches="$(set_opts
    cd "$dtap_dir"

    # Full remote main branch path
    local dtap_main_branch
    dtap_main_branch="$(cd "$dtap_dir" && git symbolic-ref refs/remotes/origin/HEAD)"

    git for-each-ref --format='%(refname) %(refname:lstrip=-1)' 'refs/remotes/origin/*' |
    while read -r refname branch_name; do
      >&2 echo "Examining $refname"

      if git symbolic-ref --quiet "$refname" >/dev/null; then
        >&2 echo 'Skipping: Branch is a symbolic ref'
        continue
      fi
      if [ "$refname" = "$dtap_main_branch" ]; then
        >&2 echo 'Skipping: Branch is main branch'
        continue
      fi
      if list_contains "$protected_branches" "$refname"; then
        >&2 echo 'Skipping: Branch is protected'
        continue
      fi
      if list_contains "$foss_branch_names" "$branch_name"; then
        >&2 echo 'Skipping: FOSS has a branch with the same name'
        continue
      fi

      files_changed_since_master="$(git diff --name-only --merge-base refs/remotes/origin/HEAD "$refname")"
      if [ "$files_changed_since_master" != "$dtap_foss_submodule" ] && [ "$files_changed_since_master" != '' ]; then
        >&2 echo 'Skipping: More files than FOSS submodule changed since main branch'
        continue
      fi

      foss_commit="$(git rev-parse --revs-only --verify "$refname:$dtap_foss_submodule")"
      # This may fail if $foss_commit is not in any branch/tag, in which case it's also not in the main branch
      if ! (cd "$foss_dir" && git merge-base --is-ancestor "$foss_commit" refs/remotes/origin/HEAD); then
        >&2 echo "Skipping: FOSS submodule commit ($foss_commit) is not in FOSS main branch"
        continue
      fi

      >&2 echo "$branch_name can be deleted"
      echo "$branch_name"
    done
  )"

  local obsolete_len
  obsolete_len="$(raw_echo_trailing_newline "$obsolete_branches" | wc -l)"
  echo "Found $obsolete_len obsolete git branches"
  raw_echo "$obsolete_branches"

  >&2 echo "$obsolete_len git branches will be deleted"

  # ==== Confirm ====
  [ -z "$no_dry_run" ] && return
  printf '\n=> %s\n' "$cleanup_description"
  confirm_deletion 'git branches'

  # ==== Delete versions ====
  >&2 printf '\nDeleting branches...\n\n'
  raw_echo_trailing_newline "$obsolete_branches" | while read -r branch_name; do
    >&2 echo "Deleting $branch_name"
    # Don't use `git push --delete` because the existing repo may not have a user with these rights
    # https://docs.gitlab.com/ee/api/branches.html#delete-repository-branch
    gitlab_dir_api "$dtap_dir" delete "repository/branches/$("$scriptdir/url.sh" encode "$branch_name")" >&2
    if [ -n "$should_delete_single" ]; then break; fi
  done

  printf '\n%s complete.\n\n' "$cleanup_description"
}



clean_foss() {
  clean_docker_build_containers
  clean_foss_packages
  clean_foss_containers
}
clean_dtap() {
  clean_dtap_branches
  clean_dtap_container_repositories  # Execute this after clean_dtap_branches so we can clean more
}

case $command in
  # Needs: curl, git, jq, yq
  clean-docker-build-containers)
    clean_docker_build_containers ;;
  # Needs: curl, git, jq
  clean-foss-packages)
    clean_foss_packages ;;
  # Needs: curl, git, jq
  clean-foss-containers)
    clean_foss_containers ;;
  clean-dtap-branches)
    clean_dtap_branches ;;
  # Needs: curl, git, jq
  clean-dtap-container-repos)
    clean_dtap_container_repositories ;;
  clean-foss)
    clean_foss ;;
  clean-dtap)
    clean_dtap ;;
  clean-all)
    clean_foss
    clean_dtap
    ;;
  *)
    >&2 echo "Unsupported command: $command"
    exit 1
esac

[ -z "$no_dry_run" ] && printf '\nThis was a dry run, nothing was deleted.\n'
[ -n "$should_delete_single" ] && printf '\n--delete-single may have prevented deletion of some objects.\n'
true
