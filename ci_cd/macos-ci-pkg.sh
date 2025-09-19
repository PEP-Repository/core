#!/usr/bin/env bash
set -euo pipefail

echo "Starting CI job script for macOS infra config adding and installer creation."

STAGING_DIR="${STAGING_DIRECTORY:-build/staging}"

if [[ -n "$CI_PROJECT_DIR" ]] && [[ -n "$PEP_FOSS_REPO_DIR" ]]; then
    PEP_MACOS_PROJ_DIR="$CI_PROJECT_DIR"
    PEP_MACOS_FOSS_DIR="$CI_PROJECT_DIR/$PEP_FOSS_REPO_DIR"
else
    echo "GitLab environment variable CI_PROJECT_DIR and/or PEP_FOSS_REPO_DIR not set, using directory relative to script."
    PEP_MACOS_PROJ_DIR=$(pwd)
    PEP_MACOS_FOSS_DIR=$(readlink -f "$(dirname -- "$0")/..")
    if [[ -z "$PEP_MACOS_FOSS_DIR" ]]; then
        echo "Error: PEP_MACOS_FOSS_DIR variable is not set."
        exit 1
    fi
fi

MACOS_SYS_ARCH=$(uname -m)

if [[ "$MACOS_SYS_ARCH" != "arm64" ]] && [[ "$MACOS_SYS_ARCH" != "x86_64" ]]; then
    echo "Error: Unable to retrieve macOS system architecture in $0"
    exit 1
fi

echo "Starting CI job script for macOS installer creation."

# Create the build/staging directory if it doesn't exist and unzip
mkdir -p "$PEP_MACOS_PROJ_DIR/$STAGING_DIR/$MACOS_SYS_ARCH"

echo "Unzipping macOS binaries."
ditto -x -k "macOS_${MACOS_SYS_ARCH}_bins.zip" "$PEP_MACOS_PROJ_DIR/$STAGING_DIR/$MACOS_SYS_ARCH/"

echo "Invoking macOS script for adding config files."
# Format: <buildDir> <configDir> <environment> <buildNumber>"
"$PEP_MACOS_FOSS_DIR/installer/macOS/macos-add-config.sh" "$PEP_MACOS_PROJ_DIR/$STAGING_DIR/$MACOS_SYS_ARCH/macOS_artifacts" "$PEP_MACOS_PROJ_DIR/config" "$CI_COMMIT_REF_NAME" "$CI_PIPELINE_ID" "$CI_JOB_ID"

# Invoking macOS installer creation script.
echo "Invoking macOS installer creation script."

echo "Reading project caption file"

CAPTION_FILE="$PEP_MACOS_PROJ_DIR/config/$CI_COMMIT_REF_NAME/project/caption.txt"
if [[ ! -f "$CAPTION_FILE" ]]; then
    echo "Cannot find project caption file at $CAPTION_FILE"
    exit 1
fi
PROJECT_CAPTION=$(<"$CAPTION_FILE")

# Create installer with the project name and environment
(
  cd "$PEP_MACOS_PROJ_DIR/$STAGING_DIR/$MACOS_SYS_ARCH/macOS_artifacts"
  "$PEP_MACOS_FOSS_DIR/installer/macOS/macos-create-installer.sh" "$MACOS_SYS_ARCH" "$PROJECT_CAPTION" "$CI_COMMIT_REF_NAME"
)

echo "Finished macOS installer creation script."