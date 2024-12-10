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
homebrew_pkgs=(cmake ninja ccache conan@2 sparkle go protoc-gen-go openssl@3)

if [[ "$(sw_vers -productVersion | awk -F '.' '{print $1}')" -lt 14 ]]; then
  echo 'Old macOS, installing llvm via brew.'
  homebrew_pkgs+=(llvm)
fi

echo "Installing/updating brew packages."
$PEP_HOMEBREW_BIN install -q "${homebrew_pkgs[@]}"

# Check if GitLab Runner is installed
if ! command -v gitlab-runner &> /dev/null; then
    echo "GitLab Runner is not installed. Installing GitLab Runner."
    $PEP_HOMEBREW_BIN install -q gitlab-runner
    
    if [[ -z "$1" ]]; then
        echo "Warning: GitLab Runner token is not provided. Please provide the token as an argument to register the runner."
        exit 1
    fi
    
    PEP_RUNNER_TOKEN="$1"
    PEP_GITLAB_URL=https://gitlab.pep.cs.ru.nl

    gitlab-runner register \
        --non-interactive \
        --url "$PEP_GITLAB_URL" \
        --token "$PEP_RUNNER_TOKEN" \
        --executor "shell" \
        --description "macos-runner"
fi

# To fully provision runner, and add the line 'pre_clone_script = "sudo chown -R <username> builds"' to the [[runners]] section of your config.toml in .gitlab-runner and using sudo visudo, add "<username> ALL=(ALL) NOPASSWD: /usr/sbin/chown" where <username> is the username of the user running the runner.

# Check if GitLab Runner is running
if ! pgrep -f "gitlab-runner" >/dev/null; then
    echo "GitLab Runner is not running. Starting GitLab Runner."
    $PEP_HOMEBREW_BIN services start gitlab-runner
fi

echo "Finished script for macOS CI runner provisioning."
