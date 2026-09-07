#!/usr/bin/env sh

# Shared POSIX sh helper functions. Source this; do not execute it:
#
#   . "$SCRIPTPATH/sh-utils.sh"

# shellcheck disable=SC2034  # Constants are used by sourcing scripts
chr_tab="$(printf '\t')"
# A cross-platform sh-compatible way to put just a newline into a variable (`$(...)` strips trailing newlines)
chr_lf="$(printf '\nX')"
chr_lf="${chr_lf%X}"
chr_crlf="$(printf '\r\nX')"
chr_crlf="${chr_crlf%X}"

fail() {
  >&2 echo "$@"
  exit 1
}

# Forwards 0/1 exit code, exits for other exit codes
# Usage: `if command_returning_0_1 || boolean; then ...; fi`, invert with `! boolean`
boolean() {
  case "$?" in
    0|1) return "$?" ;;
    *) exit "$?" ;;
  esac
}

# Echo without newlines or processed escapes (which some sh implementations do)
raw_echo() {
  printf %s "$*"
}

# Ensure nonempty line ends with a newline
raw_echo_trailing_newline() {
  str="$*"
  raw_echo "$str"
  # Nonempty & does not end with newline?
  if [ -n "$str" ] && [ "${str%"$chr_lf"}" = "$str" ]; then
    echo
  fi
}

# GNU date, for options like -d that the BSD date of macOS does not support.
# macOS needs gdate from GNU coreutils instead.
gnu_date() {
  if [ "$(uname)" = "Darwin" ]; then
    command -v gdate >/dev/null || fail 'gdate could not be found, please install coreutils using Homebrew.'
    gdate "$@"
  else
    date "$@"
  fi
}

# Convert special characters in name just like GitLab does for e.g. $CI_COMMIT_REF_SLUG.
# Based on slugify from GitLab https://gitlab.com/gitlab-org/gitlab/-/blob/9e379cc4edba7fbe4777b6b7267c43eb81cd04cd/gems/gitlab-utils/lib/gitlab/utils.rb#L56-67
slugify() {
  name="$1"
  raw_echo "$name" |
    tr '[:upper:]' '[:lower:]' |
    tr -c '[:alnum:]' '-' |  # == tr --complement
    cut -c '-63' |  # == cut --characters=-63
    sed 's/^-*//;s/-*$//'
}

# Does $string contain $substring?
contains() {
  string="$1"
  substring="$2"
  # `&& true` prevents quitting for nonzero exit code
  [ "${string#*"$substring"}" != "$string" ] && true
}

# Does the newline-separated $list contain a line exactly equal to $needle?
list_contains() {
  list="$1"
  needle="$2"
  # == grep --quiet --line-regexp --fixed-strings (long form not supported on BusyBox)
  raw_echo "$list" | grep -qxF "$needle" || boolean
}
