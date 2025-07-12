#!/usr/bin/env bash

# Variables are prefixed with "PEP" to reduce chances of naming collisions.
# Can be run locally to with a runner token to register a new runner.

echo "Starting script for macOS CI runner provisioning."

# Check if the OS is macOS and the version is at least Monterey (12.0.0)
if [[ "$(uname)" == "Darwin" && "$(sw_vers -productVersion | awk -F '.' '{print $1}')" -ge 12 ]]; then
    echo "Running on macOS Monterey or later."
else
    echo "This script requires macOS Monterey or later. Aborting."
    exit 1
fi

# Check the system architecture
if [[ "$(uname -m)" == "arm64" ]]; then
    PEP_HOMEBREW_BIN="/opt/homebrew/bin/brew"
else
    PEP_HOMEBREW_BIN="/usr/local/bin/brew"
fi

if [[ ! -x $PEP_HOMEBREW_BIN ]]; then
    echo "Installing Homebrew."
    NONINTERACTIVE=1 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    
    (echo; echo "eval \"\$($PEP_HOMEBREW_BIN shellenv)\"") >> /Users/"$(whoami)"/.zprofile
    eval "$($PEP_HOMEBREW_BIN shellenv)"
    if [[ ! -x $PEP_HOMEBREW_BIN ]]; then
        echo "Homebrew was not installed to the expected location. Aborting."
        exit 1
    fi
fi

# Check and install dependencies
homebrew_pkgs=(cmake ninja ccache conan@2 sparkle go protoc-gen-go openssl@3 jq)

if [[ "$(sw_vers -productVersion | awk -F '.' '{print $1}')" -lt 14 ]]; then
  echo 'Old macOS, installing llvm via brew.'
  homebrew_pkgs+=(llvm)
fi

echo "Installing/updating brew packages."
$PEP_HOMEBREW_BIN install -q "${homebrew_pkgs[@]}"

echo "Finished script for macOS CI runner provisioning."
