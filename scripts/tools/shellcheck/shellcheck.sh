#!/usr/bin/env bash
#
# This is just a wrapper around shellcheck. Add parameters when needed. Shellcheck is used by the CI/CD pileline.

# Shellcheck errors and warnings can be explicitly suppressed by adding a line as the one below right above the causing instruction (replace the code SC1091 with the specific code):
# shellcheck disable=SC1091

shellcheck "$@" --external-sources --severity=warning
