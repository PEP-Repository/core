#!/usr/bin/env bash
set -euo pipefail

# Assumed to be ran in a folder with the assessor_app, cli_app, download_tool_app and upload_tool_app subdirectories, each containing their corresponding .app bundles
# Moving this file will break ci_cd/macos-ci-pkg.sh

# Check if the necessary arguments are provided
if [[ $# -gt 3 ]]; then
    echo "Error: Too many arguments"
    echo "Usage: $0 MACOS_SYS_ARCH PROJECT_CAPTION CI_COMMIT_REF_NAME"
    exit 1
fi
SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
PEP_CORE_DIR="$SCRIPTPATH/../.."

MACOS_SYS_ARCH=${1:-"$(uname -m)"}
INFRA_NAME=${2:-"Local"}
BRANCH_NAME=${3:-"$($PEP_CORE_DIR/scripts/gitdir.sh get-branch-name $SCRIPTPATH)"}
CONFIG_ROOT_PATH="$($PEP_CORE_DIR/scripts/gitdir.sh get-project-root $SCRIPTPATH)"

PEP_MACOS_ASSESSOR_APP_ROOT="assessor_app"
PEP_MACOS_CLI_APP_ROOT="cli_app"
PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT="download_tool_app"
PEP_MACOS_UPLOAD_TOOL_APP_ROOT="upload_tool_app"

PEP_KEYCHAIN="$HOME/Library/Keychains/pep.keychain-db"

# Set BRANCH_NAME/CI_COMMIT_REF_NAME to test, to test the autoupdater during development, as a 
# 1.1.1 version is available online for local builds. Set the following variable to 0 to test the autoupdater during development.
PEP_MACOS_DEFAULT_VERSION=1

PEP_MACOS_INSTALLER_PATH="macos/$MACOS_SYS_ARCH"

if [[ "$INFRA_NAME" = "Local" ]]; then
  PEP_MACOS_UPDATE_PATH_PREFIX="https://pep.cs.ru.nl/Local/$PEP_MACOS_INSTALLER_PATH/update"
  source "$(git rev-parse --show-toplevel)/ci_cd/macos-ci-obtain-signing-certification.sh"
else
  PEP_MACOS_UPDATE_PATH_PREFIX="https://pep.cs.ru.nl/$INFRA_NAME/$BRANCH_NAME/$PEP_MACOS_INSTALLER_PATH/update"
fi

if [[ "$MACOS_SYS_ARCH" = "arm64" ]]; then
    MACOS_INSTALLER_ARCH="Apple Silicon"
elif [[ "$MACOS_SYS_ARCH" = "x86_64" ]]; then
    MACOS_INSTALLER_ARCH="Intel"
elif [[ "$MACOS_SYS_ARCH" = "Universal" ]]; then
    MACOS_INSTALLER_ARCH="Universal"
else
    echo "Error: Unable to retrieve macOS system architecture in $0."
    exit 1
fi

set_or_add_plist() {
    local plist_path=$1
    local plist_key=$2
    local plist_type=$3
    local plist_value=$4

    local existing_value
    existing_value=$(/usr/libexec/PlistBuddy -c "Print $plist_key" "$plist_path" 2>/dev/null) && existing_value_ec=$? || existing_value_ec=$?
    if [[ "$existing_value_ec" -eq 0 ]]; then
        if [[ "$existing_value" != "$plist_value" ]]; then
            echo "$plist_key in $plist_path is set to $existing_value, setting it to $plist_value."
            /usr/libexec/PlistBuddy -c "Set $plist_key $plist_value" "$plist_path"
        fi
    else
        echo "Adding '$plist_key: $plist_value' to $plist_path."
        /usr/libexec/PlistBuddy -c "Add $plist_key $plist_type $plist_value" "$plist_path"
    fi
}

# The signing key material should be preinstalled on the macOS runner
sign_app() {
  local app_root_dir=$1
  local app_name=$2
  local app_path="$app_root_dir/$app_name.app"
  local codesign_options=(--verbose --force --options=runtime --timestamp --keychain "$PEP_KEYCHAIN" --sign)

  # First remove any signatures found with the --deep option, if they exist
  codesign --remove-signature --deep "$app_path" || true

  # Signing Sparkle Framework
  codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/Frameworks/Sparkle.framework/Versions/B/Autoupdate"
  codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/Frameworks/Sparkle.framework/Versions/B/Updater.app/Contents/MacOS/Updater"
  codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Downloader.xpc/Contents/MacOS/Downloader"
  codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Installer.xpc/Contents/MacOS/Installer"
  codesign --verbose --force --timestamp --sign "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/Frameworks/Sparkle.framework"

  if [[ "$app_root_dir" == "$PEP_MACOS_ASSESSOR_APP_ROOT" ]]; then
    # Sign pepassessor

    # Plugins may be absent for statically linked Qt,
    # see https://doc.qt.io/qt-6/plugins-howto.html#static-plugins
    if [[ -d "$app_path/Contents/Plugins" ]]; then
      find "$app_path/Contents/Plugins" -name "*.dylib" -exec codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" {} \;
    fi
    find "$app_path/Contents/Frameworks" -maxdepth 1 -name "*.dylib" -exec codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" {} \;
    codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/MacOS/pepAssessor"
  elif [[ "$app_root_dir" == "$PEP_MACOS_CLI_APP_ROOT" ]]; then
    # Sign pepcli

    local files_to_sign=("pepcli" "pepLogon" "sparkle" "startterm.sh")

    for file in "${files_to_sign[@]}"; do
      codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/MacOS/$file"
    done
  elif [[ "$app_root_dir" == "$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT" ]]; then
    # Sign pepcli

    local files_to_sign=("pepcli" "pepLogon" "sparkle" "runpepdownload.sh")

    for file in "${files_to_sign[@]}"; do
      codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/MacOS/$file"
    done
  elif [[ "$app_root_dir" == "$PEP_MACOS_UPLOAD_TOOL_APP_ROOT" ]]; then
    # Sign pepcli

    local files_to_sign=("pepcli" "pepLogon" "sparkle" "runpepupload.sh")

    for file in "${files_to_sign[@]}"; do
      codesign "${codesign_options[@]}" "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path/Contents/MacOS/$file"
    done
  else
    echo "Error: Unknown app root dir: $app_root_dir"
    exit 1
  fi

  # Codesigning may fail if the app is already signed, so make it always return 0
  codesign --verbose --keychain "$PEP_KEYCHAIN" --deep --timestamp --sign "$GITLAB_CI_MACOS_CERTIFICATE_APP_NAME" "$app_path" || true

  # Verify the signature
  codesign --verify --verbose=4 "$app_path"
}

create_app() {
  local app_root_dir=$1
  local app_name=$2
  local app_install_location=$3
  local app_version=$4
  local app_identifier=$5

  pkgbuild --root "$app_root_dir" --analyze "${app_root_dir}Components.plist"

  set_or_add_plist "${app_root_dir}Components.plist" ":0:BundleIsRelocatable" "bool" "false"

  sign_app "$app_root_dir" "$app_name"

  pkgbuild --root "$app_root_dir" \
            --keychain "$PEP_KEYCHAIN" \
            --component-plist "${app_root_dir}Components.plist" \
            --identifier "$app_identifier" \
            --ownership "preserve" \
            --version "$app_version" \
            --install-location "$app_install_location" \
            --sign "$GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME" \
            --timestamp \
            "$app_name.pkg"

  # Verify the signature
  pkgutil --check-signature "$app_name.pkg"
}

if ! PEP_MACOS_ASSESSOR_APP_PLIST_PATH=$(readlink -f "$PEP_MACOS_ASSESSOR_APP_ROOT"/*.app/Contents/Info.plist); then
  echo "Error: Could not find Info.plist for root dir: $PEP_MACOS_ASSESSOR_APP_ROOT"
fi

if ! PEP_MACOS_CLI_APP_PLIST_PATH=$(readlink -f "$PEP_MACOS_CLI_APP_ROOT"/*.app/Contents/Info.plist); then
  echo "Error: Could not find Info.plist for root dir: $PEP_MACOS_CLI_APP_ROOT"
fi

if ! PEP_MACOS_DOWNLOAD_TOOL_APP_PLIST_PATH=$(readlink -f "$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT"/*.app/Contents/Info.plist); then
  echo "Error: Could not find Info.plist for root dir: $PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT"
fi

if ! PEP_MACOS_UPLOAD_TOOL_APP_PLIST_PATH=$(readlink -f "$PEP_MACOS_UPLOAD_TOOL_APP_ROOT"/*.app/Contents/Info.plist); then
  echo "Error: Could not find Info.plist for root dir: $PEP_MACOS_UPLOAD_TOOL_APP_ROOT"
fi

PEP_MACOS_ASSESSOR_APP_NAME=$(/usr/libexec/PlistBuddy -c "Print CFBundleDisplayName" "$PEP_MACOS_ASSESSOR_APP_PLIST_PATH")
PEP_MACOS_ASSESSOR_APP_IDENTIFIER=$(/usr/libexec/PlistBuddy -c "Print CFBundleIdentifier" "$PEP_MACOS_ASSESSOR_APP_PLIST_PATH")
PEP_MACOS_ASSESSOR_APP_INSTALL_LOCATION="/Applications"

PEP_MACOS_CLI_APP_NAME=$(/usr/libexec/PlistBuddy -c "Print CFBundleDisplayName" "$PEP_MACOS_CLI_APP_PLIST_PATH")
PEP_MACOS_CLI_APP_IDENTIFIER=$(/usr/libexec/PlistBuddy -c "Print CFBundleIdentifier" "$PEP_MACOS_CLI_APP_PLIST_PATH")
PEP_MACOS_CLI_APP_INSTALL_LOCATION="/Applications"

PEP_MACOS_DOWNLOAD_TOOL_APP_NAME=$(/usr/libexec/PlistBuddy -c "Print CFBundleDisplayName" "$PEP_MACOS_DOWNLOAD_TOOL_APP_PLIST_PATH")
PEP_MACOS_DOWNLOAD_TOOL_APP_IDENTIFIER=$(/usr/libexec/PlistBuddy -c "Print CFBundleIdentifier" "$PEP_MACOS_DOWNLOAD_TOOL_APP_PLIST_PATH")
PEP_MACOS_DOWNLOAD_TOOL_APP_INSTALL_LOCATION="/Applications"

PEP_MACOS_UPLOAD_TOOL_APP_NAME=$(/usr/libexec/PlistBuddy -c "Print CFBundleDisplayName" "$PEP_MACOS_UPLOAD_TOOL_APP_PLIST_PATH")
PEP_MACOS_UPLOAD_TOOL_APP_IDENTIFIER=$(/usr/libexec/PlistBuddy -c "Print CFBundleIdentifier" "$PEP_MACOS_UPLOAD_TOOL_APP_PLIST_PATH")
PEP_MACOS_UPLOAD_TOOL_APP_INSTALL_LOCATION="/Applications"

PEP_MACOS_ASSESSOR_APP_INFRA_NAME="$PEP_MACOS_ASSESSOR_APP_NAME"
PEP_MACOS_CLI_APP_INFRA_NAME="$PEP_MACOS_CLI_APP_NAME"
PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME="$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME"
PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME="$PEP_MACOS_UPLOAD_TOOL_APP_NAME"

# Update name to infra names for non-universal apps (universal is already at infra name by now)
if [[ "$MACOS_INSTALLER_ARCH" != "Universal" ]] && [[ "$PEP_MACOS_ASSESSOR_APP_NAME" != *"$INFRA_NAME"* ]] && [[ "$PEP_MACOS_ASSESSOR_APP_NAME" != *"$BRANCH_NAME"* ]]; then
  PEP_MACOS_ASSESSOR_APP_INFRA_NAME="$PEP_MACOS_ASSESSOR_APP_NAME ($INFRA_NAME $BRANCH_NAME)"
  set_or_add_plist "$PEP_MACOS_ASSESSOR_APP_PLIST_PATH" "CFBundleDisplayName" "string" "$PEP_MACOS_ASSESSOR_APP_INFRA_NAME"
  mv "$PEP_MACOS_ASSESSOR_APP_ROOT/$PEP_MACOS_ASSESSOR_APP_NAME.app" "$PEP_MACOS_ASSESSOR_APP_ROOT/$PEP_MACOS_ASSESSOR_APP_INFRA_NAME.app"
  PEP_MACOS_ASSESSOR_APP_PLIST_PATH="$PEP_MACOS_ASSESSOR_APP_ROOT/$PEP_MACOS_ASSESSOR_APP_INFRA_NAME.app/Contents/Info.plist"

  PEP_MACOS_CLI_APP_INFRA_NAME="$PEP_MACOS_CLI_APP_NAME ($INFRA_NAME $BRANCH_NAME)"
  set_or_add_plist "$PEP_MACOS_CLI_APP_PLIST_PATH" "CFBundleDisplayName" "string" "$PEP_MACOS_CLI_APP_INFRA_NAME"
  mv "$PEP_MACOS_CLI_APP_ROOT/$PEP_MACOS_CLI_APP_NAME.app" "$PEP_MACOS_CLI_APP_ROOT/$PEP_MACOS_CLI_APP_INFRA_NAME.app"
  PEP_MACOS_CLI_APP_PLIST_PATH="$PEP_MACOS_CLI_APP_ROOT/$PEP_MACOS_CLI_APP_INFRA_NAME.app/Contents/Info.plist"

  PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME="$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME ($INFRA_NAME $BRANCH_NAME)"
  set_or_add_plist "$PEP_MACOS_DOWNLOAD_TOOL_APP_PLIST_PATH" "CFBundleDisplayName" "string" "$PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME"
  mv "$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT/$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME.app" "$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT/$PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME.app"
  PEP_MACOS_DOWNLOAD_TOOL_APP_PLIST_PATH="$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT/$PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME.app/Contents/Info.plist"

  PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME="$PEP_MACOS_UPLOAD_TOOL_APP_NAME ($INFRA_NAME $BRANCH_NAME)"
  set_or_add_plist "$PEP_MACOS_UPLOAD_TOOL_APP_PLIST_PATH" "CFBundleDisplayName" "string" "$PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME"
  mv "$PEP_MACOS_UPLOAD_TOOL_APP_ROOT/$PEP_MACOS_UPLOAD_TOOL_APP_NAME.app" "$PEP_MACOS_UPLOAD_TOOL_APP_ROOT/$PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME.app"
  PEP_MACOS_UPLOAD_TOOL_APP_PLIST_PATH="$PEP_MACOS_UPLOAD_TOOL_APP_ROOT/$PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME.app/Contents/Info.plist"
fi

# Update versions
CONFIG_VERSION_MAJOR="$($PEP_CORE_DIR/scripts/parse-version.sh    get-major    "$CONFIG_ROOT_PATH/version.json" "${CI_PIPELINE_ID:-$PEP_MACOS_DEFAULT_VERSION}" "$PEP_MACOS_APP_CONFIG_VERSION.${CI_JOB_ID:-$PEP_MACOS_DEFAULT_VERSION}")"
CONFIG_VERSION_MINOR="$($PEP_CORE_DIR/scripts/parse-version.sh    get-minor    "$CONFIG_ROOT_PATH/version.json" "${CI_PIPELINE_ID:-$PEP_MACOS_DEFAULT_VERSION}" "$PEP_MACOS_APP_CONFIG_VERSION.${CI_JOB_ID:-$PEP_MACOS_DEFAULT_VERSION}")"
CONFIG_VERSION_BUILD="$($PEP_CORE_DIR/scripts/parse-version.sh    get-build    "$CONFIG_ROOT_PATH/version.json" "${CI_PIPELINE_ID:-$PEP_MACOS_DEFAULT_VERSION}" "$PEP_MACOS_APP_CONFIG_VERSION.${CI_JOB_ID:-$PEP_MACOS_DEFAULT_VERSION}")"
CONFIG_VERSION_REVISION="$($PEP_CORE_DIR/scripts/parse-version.sh get-revision "$CONFIG_ROOT_PATH/version.json" "${CI_PIPELINE_ID:-$PEP_MACOS_DEFAULT_VERSION}" "$PEP_MACOS_APP_CONFIG_VERSION.${CI_JOB_ID:-$PEP_MACOS_DEFAULT_VERSION}")"

PEP_MACOS_APP_CONFIG_VERSION="$CONFIG_VERSION_MAJOR.$CONFIG_VERSION_MINOR.$CONFIG_VERSION_BUILD"
PEP_MACOS_APP_CONFIG_VERSION_LONG="$PEP_MACOS_APP_CONFIG_VERSION.$CONFIG_VERSION_REVISION"

set_or_add_plist "$PEP_MACOS_ASSESSOR_APP_PLIST_PATH" "CFBundleVersion" "string" "$PEP_MACOS_APP_CONFIG_VERSION_LONG"
set_or_add_plist "$PEP_MACOS_ASSESSOR_APP_PLIST_PATH" "CFBundleShortVersionString" "string" "$PEP_MACOS_APP_CONFIG_VERSION"

set_or_add_plist "$PEP_MACOS_CLI_APP_PLIST_PATH" "CFBundleVersion" "string" "$PEP_MACOS_APP_CONFIG_VERSION_LONG"
set_or_add_plist "$PEP_MACOS_CLI_APP_PLIST_PATH" "CFBundleShortVersionString" "string" "$PEP_MACOS_APP_CONFIG_VERSION"

set_or_add_plist "$PEP_MACOS_DOWNLOAD_TOOL_APP_PLIST_PATH" "CFBundleVersion" "string" "$PEP_MACOS_APP_CONFIG_VERSION_LONG"
set_or_add_plist "$PEP_MACOS_DOWNLOAD_TOOL_APP_PLIST_PATH" "CFBundleShortVersionString" "string" "$PEP_MACOS_APP_CONFIG_VERSION"

set_or_add_plist "$PEP_MACOS_UPLOAD_TOOL_APP_PLIST_PATH" "CFBundleVersion" "string" "$PEP_MACOS_APP_CONFIG_VERSION_LONG"
set_or_add_plist "$PEP_MACOS_UPLOAD_TOOL_APP_PLIST_PATH" "CFBundleShortVersionString" "string" "$PEP_MACOS_APP_CONFIG_VERSION"

# Update appcast urls
PEP_MACOS_ASSESSOR_APPCAST_URL="$PEP_MACOS_UPDATE_PATH_PREFIX/$PEP_MACOS_ASSESSOR_APP_ROOT/AppCast.xml"
set_or_add_plist "$PEP_MACOS_ASSESSOR_APP_PLIST_PATH" "SUFeedURL" "string" "$PEP_MACOS_ASSESSOR_APPCAST_URL"

PEP_MACOS_CLI_APPCAST_URL="$PEP_MACOS_UPDATE_PATH_PREFIX/$PEP_MACOS_CLI_APP_ROOT/AppCast.xml"
set_or_add_plist "$PEP_MACOS_CLI_APP_PLIST_PATH" "SUFeedURL" "string" "$PEP_MACOS_CLI_APPCAST_URL"

PEP_MACOS_DOWNLOAD_TOOL_APPCAST_URL="$PEP_MACOS_UPDATE_PATH_PREFIX/$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT/AppCast.xml"
set_or_add_plist "$PEP_MACOS_DOWNLOAD_TOOL_APP_PLIST_PATH" "SUFeedURL" "string" "$PEP_MACOS_DOWNLOAD_TOOL_APPCAST_URL"

PEP_MACOS_UPLOAD_TOOL_APPCAST_URL="$PEP_MACOS_UPDATE_PATH_PREFIX/$PEP_MACOS_UPLOAD_TOOL_APP_ROOT/AppCast.xml"
set_or_add_plist "$PEP_MACOS_UPLOAD_TOOL_APP_PLIST_PATH" "SUFeedURL" "string" "$PEP_MACOS_UPLOAD_TOOL_APPCAST_URL"

# Create PEP Assessor app
create_app "$PEP_MACOS_ASSESSOR_APP_ROOT" "$PEP_MACOS_ASSESSOR_APP_INFRA_NAME" "$PEP_MACOS_ASSESSOR_APP_INSTALL_LOCATION" "$PEP_MACOS_APP_CONFIG_VERSION_LONG" "$PEP_MACOS_ASSESSOR_APP_IDENTIFIER"

# Create PEP Command Line Interface app
create_app "$PEP_MACOS_CLI_APP_ROOT" "$PEP_MACOS_CLI_APP_INFRA_NAME" "$PEP_MACOS_CLI_APP_INSTALL_LOCATION" "$PEP_MACOS_APP_CONFIG_VERSION_LONG" "$PEP_MACOS_CLI_APP_IDENTIFIER"

# Create PEP Download Tool app
create_app "$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT" "$PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME" "$PEP_MACOS_DOWNLOAD_TOOL_APP_INSTALL_LOCATION" "$PEP_MACOS_APP_CONFIG_VERSION_LONG" "$PEP_MACOS_DOWNLOAD_TOOL_APP_IDENTIFIER"

# Create PEP Upload Tool app
create_app "$PEP_MACOS_UPLOAD_TOOL_APP_ROOT" "$PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME" "$PEP_MACOS_UPLOAD_TOOL_APP_INSTALL_LOCATION" "$PEP_MACOS_APP_CONFIG_VERSION_LONG" "$PEP_MACOS_UPLOAD_TOOL_APP_IDENTIFIER"

# New installer dirs
mkdir -p "installer/$PEP_MACOS_INSTALLER_PATH/update/$PEP_MACOS_ASSESSOR_APP_ROOT"
mkdir -p "installer/$PEP_MACOS_INSTALLER_PATH/update/$PEP_MACOS_CLI_APP_ROOT"
mkdir -p "installer/$PEP_MACOS_INSTALLER_PATH/update/$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT"
mkdir -p "installer/$PEP_MACOS_INSTALLER_PATH/update/$PEP_MACOS_UPLOAD_TOOL_APP_ROOT"

# Move signed apps, to installer directory, to be used as update files
ditto "$PEP_MACOS_ASSESSOR_APP_ROOT/$PEP_MACOS_ASSESSOR_APP_INFRA_NAME.app" "installer/$PEP_MACOS_INSTALLER_PATH/update/$PEP_MACOS_ASSESSOR_APP_ROOT/$PEP_MACOS_ASSESSOR_APP_INFRA_NAME.app"
ditto "$PEP_MACOS_CLI_APP_ROOT/$PEP_MACOS_CLI_APP_INFRA_NAME.app" "installer/$PEP_MACOS_INSTALLER_PATH/update/$PEP_MACOS_CLI_APP_ROOT/$PEP_MACOS_CLI_APP_INFRA_NAME.app"
ditto "$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT/$PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME.app" "installer/$PEP_MACOS_INSTALLER_PATH/update/$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT/$PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME.app"
ditto "$PEP_MACOS_UPLOAD_TOOL_APP_ROOT/$PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME.app" "installer/$PEP_MACOS_INSTALLER_PATH/update/$PEP_MACOS_UPLOAD_TOOL_APP_ROOT/$PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME.app"

# Combine the pkg to a complete installer pkg
cat > "Distribution.dist" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>PEP</title>
    <options customize="always" require-scripts="false"/>
    <choices-outline>
        <line choice="$PEP_MACOS_ASSESSOR_APP_ROOT"/>
        <line choice="$PEP_MACOS_CLI_APP_ROOT"/>
        <line choice="$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT"/>
        <line choice="$PEP_MACOS_UPLOAD_TOOL_APP_ROOT"/>
    </choices-outline>
    <choice id="$PEP_MACOS_ASSESSOR_APP_ROOT" title="$PEP_MACOS_ASSESSOR_APP_NAME" description="$PEP_MACOS_ASSESSOR_APP_NAME">
        <pkg-ref id="$PEP_MACOS_ASSESSOR_APP_IDENTIFIER" version="$PEP_MACOS_APP_CONFIG_VERSION" onConclusion="none">$PEP_MACOS_ASSESSOR_APP_INFRA_NAME.pkg</pkg-ref>
    </choice>
    <choice id="$PEP_MACOS_CLI_APP_ROOT" title="$PEP_MACOS_CLI_APP_NAME" description="$PEP_MACOS_CLI_APP_NAME">
        <pkg-ref id="$PEP_MACOS_CLI_APP_IDENTIFIER" version="$PEP_MACOS_APP_CONFIG_VERSION" onConclusion="none">$PEP_MACOS_CLI_APP_INFRA_NAME.pkg</pkg-ref>
    </choice>
    <choice id="$PEP_MACOS_DOWNLOAD_TOOL_APP_ROOT" title="$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME" description="$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME">
        <pkg-ref id="$PEP_MACOS_DOWNLOAD_TOOL_APP_IDENTIFIER" version="$PEP_MACOS_APP_CONFIG_VERSION" onConclusion="none">$PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME.pkg</pkg-ref>
    </choice>
    <choice id="$PEP_MACOS_UPLOAD_TOOL_APP_ROOT" title="$PEP_MACOS_UPLOAD_TOOL_APP_NAME" description="$PEP_MACOS_UPLOAD_TOOL_APP_NAME">
        <pkg-ref id="$PEP_MACOS_UPLOAD_TOOL_APP_IDENTIFIER" version="$PEP_MACOS_APP_CONFIG_VERSION" onConclusion="none">$PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME.pkg</pkg-ref>
    </choice>
</installer-gui-script>
EOF

INSTALLER_NAME="PEPInstaller.pkg"

# Create the distribution pkg with productbuild
productbuild  --distribution Distribution.dist \
              --keychain "$PEP_KEYCHAIN" \
              --timestamp \
              --sign "$GITLAB_CI_MACOS_CERTIFICATE_INSTALL_NAME" \
              "$INSTALLER_NAME"

# Remove individual installer pkgs
rm -f "$PEP_MACOS_ASSESSOR_APP_INFRA_NAME.pkg"
rm -f "$PEP_MACOS_CLI_APP_INFRA_NAME.pkg"
rm -f "$PEP_MACOS_DOWNLOAD_TOOL_APP_INFRA_NAME.pkg"
rm -f "$PEP_MACOS_UPLOAD_TOOL_APP_INFRA_NAME.pkg"

# Verify the signature
pkgutil --check-signature "$INSTALLER_NAME"

# Move the main installer pkg to the installer dir
mv "$INSTALLER_NAME" "installer/$PEP_MACOS_INSTALLER_PATH/$INSTALLER_NAME"