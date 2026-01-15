#!/usr/bin/env bash
# shellcheck disable=SC2207
# As mapfile is not available on macos

set -euo pipefail

# Moving this file will break ci_cd/macos-ci-pkg.sh

# Set variables
PEP_MACOS_ROOT_DIR=$(readlink -f "$(dirname -- "$0")/../..")

# Get absolute build path and config paths
BUILD_PATH="$1"
CONFIG_DIR="$2"

# Check if the directories exist
if [[ ! -d "$BUILD_PATH" ]]; then
    echo "Build path does not exist: $BUILD_PATH"
    exit 1
fi

if [[ ! -d "$CONFIG_DIR" ]]; then
    echo "Config directory does not exist: $CONFIG_DIR"
    exit 1
fi

IFS=$'\n' apps_ass=($(find "$BUILD_PATH/assessor_app" -maxdepth 1 -name "*.app"))
IFS=$'\n' apps_cli=($(find "$BUILD_PATH/cli_app" -maxdepth 1 -name "*.app"))
IFS=$'\n' apps_dt=($(find "$BUILD_PATH/download_tool_app" -maxdepth 1 -name "*.app"))
IFS=$'\n' apps_ut=($(find "$BUILD_PATH/upload_tool_app" -maxdepth 1 -name "*.app"))

# Check that exactly one .app file was found in each directory
if [[ ${#apps_ass[@]} -ne 1 ]] || [[ ${#apps_cli[@]} -ne 1 ]] || [[ ${#apps_dt[@]} -ne 1 ]] || [[ ${#apps_ut[@]} -ne 1 ]]; then
    echo "Error: Expected exactly one .app file in each directory, but found a different number"
    exit 1
fi

PEP_MACOS_ASSESSOR_APP_NAME="$(basename "${apps_ass[0]%.app}")"
PEP_MACOS_CLI_APP_NAME="$(basename "${apps_cli[0]%.app}")"
PEP_MACOS_DOWNLOAD_TOOL_APP_NAME="$(basename "${apps_dt[0]%.app}")"
PEP_MACOS_UPLOAD_TOOL_APP_NAME="$(basename "${apps_ut[0]%.app}")"

ASSESSOR_RESOURCES_DIR="$BUILD_PATH/assessor_app/$PEP_MACOS_ASSESSOR_APP_NAME.app/Contents/Resources"
CLI_RESOURCES_DIR="$BUILD_PATH/cli_app/$PEP_MACOS_CLI_APP_NAME.app/Contents/Resources"
DOWNLOAD_TOOL_RESOURCES_DIR="$BUILD_PATH/download_tool_app/$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME.app/Contents/Resources"
UPLOAD_TOOL_RESOURCES_DIR="$BUILD_PATH/upload_tool_app/$PEP_MACOS_UPLOAD_TOOL_APP_NAME.app/Contents/Resources"

# Commit ref name
ENV_NAME=$3

PIPELINE_ID="${4:-${CI_PIPELINE_ID:-}}"
JOB_ID="${5:-${CI_JOB_ID:-}}"

INFRA_DIR="$CONFIG_DIR/$ENV_NAME"

copy_config_files() {
    local target_dir=$1

    echo "Creating Gitlab version JSON"
    "$PEP_MACOS_ROOT_DIR/scripts/createConfigVersionJson.sh" "$INFRA_DIR" "$INFRA_DIR/project" "$PIPELINE_ID" "$JOB_ID" > "$target_dir/configVersion.json"

    if [[ ! -f "$target_dir/configVersion.json" ]]; then
        echo "Config version file was not created at $target_dir/configVersion.json"
        exit 1
    fi

    echo "Copying infrastructure configuration files"
    cp "$INFRA_DIR/rootCA.cert" "$target_dir/"
    cp "$INFRA_DIR/ShadowAdministration.pub" "$target_dir/"
    cp "$INFRA_DIR/client/ClientConfig.json" "$target_dir/"

    echo "Copying project configuration files"
    cp "$INFRA_DIR/project/client/"* "$target_dir/"

    echo "Finished copying configuration files."
}

copy_config_files "$ASSESSOR_RESOURCES_DIR"
copy_config_files "$CLI_RESOURCES_DIR"
copy_config_files "$DOWNLOAD_TOOL_RESOURCES_DIR"
copy_config_files "$UPLOAD_TOOL_RESOURCES_DIR"
