#!/usr/bin/env bash

# Special token groups: "Research Assessor", "Access Administrator",  "RegistrationServer", "Data Administrator"
# shellcheck disable=SC2034
TOKENGROUP="Research Assessor"

# Call PEPClient via another shell script.
# shellcheck disable=SC1091
source pepdo.sh