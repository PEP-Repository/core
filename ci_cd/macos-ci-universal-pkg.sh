#!/usr/bin/env bash
set -e

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

# Create universal binary
(
echo "Invoking macOS creation script for universal binary."
cd "$PEP_MACOS_PROJ_DIR/$STAGING_DIR/"
"$PEP_MACOS_FOSS_DIR/installer/macOS/macos-create-universal-binary.sh"
echo "Finished macOS creation script for universal binary."
)

echo "Reading project caption file"

CAPTION_FILE="$PEP_MACOS_PROJ_DIR/config/$CI_COMMIT_REF_NAME/project/caption.txt"
if [[ ! -f "$CAPTION_FILE" ]]; then
    echo "Cannot find project caption file at $CAPTION_FILE"
    exit 1
fi
PROJECT_CAPTION=$(<"$CAPTION_FILE")

# Invoking macOS installer creation script.
(
echo "Invoking macOS installer creation script for universal binary."
cd "$PEP_MACOS_PROJ_DIR/$STAGING_DIR/universal/macOS_artifacts"
"$PEP_MACOS_FOSS_DIR/installer/macOS/macos-create-installer.sh" "Universal" "$PROJECT_CAPTION" "$CI_COMMIT_REF_NAME" 
echo "Finished macOS installer creation script for universal binary."
)



