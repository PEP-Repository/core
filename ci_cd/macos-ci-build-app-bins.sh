#!/usr/bin/env bash
# Packages the macOS apps (PEP Assessor, pepcli and derivatives) into a zip file, from an existing build of the
# pepLogon, pepAssessor and pepcli targets (see the macos-client job in .gitlab-ci.yml).
# For local use, pass the build type (default: Debug); the build is then expected in build/<build type>.
set -eo pipefail

BUILD_DIR="${BUILD_DIRECTORY:-build}"

echo "Starting CI job script for creation of macOS binaries."

if [[ -n "$CI_PROJECT_DIR" ]]; then
    PEP_MACOS_ROOT_DIR="$CI_PROJECT_DIR"
else
    echo "GitLab environment variable CI_PROJECT_DIR not set, using directory relative to script."
    # portability-disable=readlink-f # This script only ever runs on macOS CI, where readlink -f exists
    PEP_MACOS_ROOT_DIR=$(readlink -f "$(dirname -- "$0")/..")
fi

# Development helper: check if CI_COMMIT_REF_NAME is specified, if not, set it to "local". Local can use incremental builds.
if [[ -z "$CI_COMMIT_REF_NAME" ]]; then
    echo "No CI_COMMIT_REF_NAME specified: performing 'local' build."
    CI_COMMIT_REF_NAME="local"
fi

echo "Creating macOS CI artifacts for $CI_COMMIT_REF_NAME build."

if [[ "$CI_COMMIT_REF_NAME" == "local" ]]; then
  BUILD_TYPE_DIR="${1:-Debug}"
else
  BUILD_TYPE_DIR="."
fi

# Apparently conan scripts use unset variables, so temporarily disable check for this
set +u
# Add macdeployqt etc. to PATH
# Suppress shellcheck
# shellcheck source=/dev/null
. "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/generators/conanbuild.sh"
set -u

echo "Removing old macOS_artifacts folder."
rm -rf "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts"

PEP_MACOS_ASSESSOR_APP_NAME="PEP Assessor"
PEP_MACOS_ASSESSOR_APP_BUILD_ROOT_DIR="$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/assessor_app/$PEP_MACOS_ASSESSOR_APP_NAME.app"

mkdir -p "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/assessor_app"

# Copy to staging dir and rename app
cp -R "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/cpp/pep/assessor/pepAssessor.app" "$PEP_MACOS_ASSESSOR_APP_BUILD_ROOT_DIR"

# Create Info.plist for pepAssessor
(
    cd "$PEP_MACOS_ASSESSOR_APP_BUILD_ROOT_DIR/Contents/"
    "$PEP_MACOS_ROOT_DIR/installer/macOS/macos-create-info-plist.sh" "$PEP_MACOS_ASSESSOR_APP_NAME"
)

# The QT deployment tool, macdeployqt, is broken for homebrew, see: https://github.com/orgs/Homebrew/discussions/2823
# A manual replacement of this tool was created in copy-qt-dylibs.sh, but for Conan it is not necessary anymore,
# and it tries to use homebrew Qt libs.
# It would be called like this: "$PEP_MACOS_ROOT_DIR/installer/macOS/copy-qt-dylibs.sh" "$PEP_MACOS_ASSESSOR_APP_BUILD_ROOT_DIR/Contents/MacOS/pepAssessor"

# Run macdeployqt on pepAssessor
echo "Running macdeployqt on $PEP_MACOS_ASSESSOR_APP_BUILD_ROOT_DIR."

# Signing the rest of the app using macdeployqt
macdeployqt "$PEP_MACOS_ASSESSOR_APP_BUILD_ROOT_DIR"

# Function to create pepcli app and derivate(s)
create_cli_app() {
    local app_name="$1"
    local build_root_dir="$2"
    local app_icon_path="$3"
    local start_script="$4"
    local include_autocompletion="${6:-true}"

    echo "Creating $app_name app."

    mkdir -p "$build_root_dir"/Contents/{MacOS,Resources,Frameworks}

    cp "$app_icon_path" "$build_root_dir/Contents/Resources/appicon.icns"

    # Copy start script based on its type
    if [[ "$start_script" == *"startterm.sh" ]]; then
        # Script starts terminal session
        cp "$start_script" "$build_root_dir/Contents/MacOS/"
        # and executes this script for pepcli
        cp "$PEP_MACOS_ROOT_DIR/installer/macOS/runpepcli.sh" "$build_root_dir/Contents/Resources/"
    else
        # Direct script execution (e.g., runpepdownload.sh)
        cp "$start_script" "$build_root_dir/Contents/MacOS/"
    fi

    echo "Copying pepcli executables."
    cp "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR"/cpp/pep/cli/{pepcli,sparkle} "$build_root_dir/Contents/MacOS/"

    echo "Copying pepcli frameworks."
    cp -R "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/cpp/pep/cli/Frameworks/"* "$build_root_dir/Contents/Frameworks/"

    if [[ "$CI_COMMIT_REF_NAME" == "local" ]]; then
        echo "Copying local config."
        cp "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR"/cpp/pep/cli/{ClientConfig.json,rootCA.cert,ShadowAdministration.pub} "$build_root_dir/Contents/Resources/"
        cp "$PEP_MACOS_ROOT_DIR/config/local/authserver/AuthserverHTTPSCertificate.pem" "$build_root_dir/Contents/Resources/"
    fi

    echo "Copying peplogon."
    cp "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/cpp/pep/logon/pepLogon" "$build_root_dir/Contents/MacOS/"

    if [[ "$include_autocompletion" == "true" ]]; then
        echo "Copying pepcli autocompletion."
        cp -r "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR"/autocomplete/{autocomplete_pep.bash,autocomplete_pep.zsh} "$build_root_dir/Contents/Resources/"
    fi

    # Create Info.plist
    (
        cd "$build_root_dir/Contents"
        "$PEP_MACOS_ROOT_DIR/installer/macOS/macos-create-info-plist.sh" "$app_name"
    )
}

PEP_MACOS_CLI_APP_NAME="PEP Command Line Interface"
PEP_MACOS_CLI_APP_BUILD_ROOT_DIR="$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/cli_app/$PEP_MACOS_CLI_APP_NAME.app"

create_cli_app \
    "$PEP_MACOS_CLI_APP_NAME" \
    "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR" \
    "$PEP_MACOS_ROOT_DIR/cpp/pep/assessor/guilib/appiconcli.icns" \
    "$PEP_MACOS_ROOT_DIR/installer/macOS/startterm.sh" \
    "true"

PEP_MACOS_DOWNLOAD_TOOL_APP_NAME="PEP Download Tool"
PEP_MACOS_DOWNLOAD_TOOL_APP_BUILD_ROOT_DIR="$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/download_tool_app/$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME.app"

create_cli_app \
    "$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME" \
    "$PEP_MACOS_DOWNLOAD_TOOL_APP_BUILD_ROOT_DIR" \
    "$PEP_MACOS_ROOT_DIR/cpp/pep/assessor/guilib/appicondt.icns" \
    "$PEP_MACOS_ROOT_DIR/installer/macOS/runpepdownload.sh" \
    "false"

PEP_MACOS_UPLOAD_TOOL_APP_NAME="PEP Upload Tool"
PEP_MACOS_UPLOAD_TOOL_APP_BUILD_ROOT_DIR="$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/upload_tool_app/$PEP_MACOS_UPLOAD_TOOL_APP_NAME.app"

create_cli_app \
    "$PEP_MACOS_UPLOAD_TOOL_APP_NAME" \
    "$PEP_MACOS_UPLOAD_TOOL_APP_BUILD_ROOT_DIR" \
    "$PEP_MACOS_ROOT_DIR/cpp/pep/assessor/guilib/appicon.icns" \
    "$PEP_MACOS_ROOT_DIR/installer/macOS/runpepupload.sh" \
    "false"

MACOS_SYS_ARCH=$(uname -m)

if [[ "$MACOS_SYS_ARCH" != "arm64" ]] && [[ "$MACOS_SYS_ARCH" != "x86_64" ]]; then
    echo "Error: Unable to retrieve macOS system architecture in $0"
    exit 1
fi

# Bundle Sparkle's generate_appcast tool into the binaries zip, so the macos-generate-appcast CI job can use it without
# invoking Conan or Homebrew. The cli CMake target copies it next to pepcli from the Sparkle Conan package
echo "Staging generate_appcast tool."
GENERATE_APPCAST_SRC="$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/cpp/pep/cli/generate_appcast"
if [[ ! -f "$GENERATE_APPCAST_SRC" ]]; then
    echo "Error: generate_appcast not found at $GENERATE_APPCAST_SRC"
    exit 1
fi
mkdir -p "$BUILD_DIR/macOS_artifacts/sparkle-tools"
cp "$GENERATE_APPCAST_SRC" "$BUILD_DIR/macOS_artifacts/sparkle-tools/generate_appcast"

echo "Creating zip file with macOS binaries."

# -c: Create an archive. -k: Create a PKZip archive (.zip)
ditto -c -k --keepParent "$BUILD_DIR/macOS_artifacts" "$BUILD_DIR/macOS_${MACOS_SYS_ARCH}_bins.zip"

# Finished CI job script for macOS installer creation.
echo "Finished CI job script for creation of macOS binaries."
