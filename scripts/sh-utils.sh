#!/usr/bin/env sh

# Shared POSIX sh helper functions. Source this; do not execute it:
#
#   . "$SCRIPTPATH/sh-utils.sh"

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
  printf '%s\n' "$list" | grep -qxF "$needle"
}
