#!/usr/bin/env bash
set -eo pipefail

BUILD_DIR="${BUILD_DIRECTORY:-build}"

echo "Starting CI job script for creation of macOS binaries."
export CMAKE_COLOR_DIAGNOSTICS=ON  # Let CMake pass -fcolor-diagnostics
export CLICOLOR_FORCE=1  # Colored output for e.g. Conan & Ninja (otherwise -fcolor-diagnostics still won't work)

if [[ -n "$CI_PROJECT_DIR" ]]; then
    PEP_MACOS_ROOT_DIR="$CI_PROJECT_DIR"
else
    echo "GitLab environment variable CI_PROJECT_DIR not set, using directory relative to script."
    PEP_MACOS_ROOT_DIR=$(readlink -f "$(dirname -- "$0")/..")
fi

# Development helper: check if CI_COMMIT_REF_NAME is specified, if not, set it to "Local". Local can use incremental builds.
if [[ -z "$CI_COMMIT_REF_NAME" ]]; then
    echo "No CI_COMMIT_REF_NAME specified: performing 'Local' build."
    CI_COMMIT_REF_NAME="Local"
else
    # For nonlocal builds: invoking macOS CI runner provisioning script.
    "$PEP_MACOS_ROOT_DIR/ci_cd/macos-ci-runner.sh"
fi

echo "Creating macOS CI artifacts for $CI_COMMIT_REF_NAME build."

# Check if the "build" directory already exists.
if [[ -d "$PEP_MACOS_ROOT_DIR/$BUILD_DIR" ]]; then
    # If CI_COMMIT_REF_NAME is not "Local", exit with an error message.
    if [[ "$CI_COMMIT_REF_NAME" != "Local" ]]; then
        echo "Build directory $PEP_MACOS_ROOT_DIR/$BUILD_DIR already exists."
    else
        echo "Performing incremental build on existing build directory."
    fi
fi

if [[ "$CI_COMMIT_REF_NAME" == "Local" ]]; then
  LOCAL_BUILD_INFRA=${1:-local}
  LOCAL_BUILD_TYPE=${2:-Debug}
  MACOS_CMAKE_CONFIGURE_PRESET="$(tr "[:upper:]" "[:lower:]" <<< "$LOCAL_BUILD_INFRA"_"$LOCAL_BUILD_TYPE")"
  MACOS_CMAKE_BUILD_PRESET="${MACOS_CMAKE_CONFIGURE_PRESET}_build"
  conan install ./ --build=missing -s:a build_type="$LOCAL_BUILD_TYPE"
else
  export CC=clang CXX=clang++
  if [[ "$(sw_vers -productVersion | awk -F '.' '{print $1}')" -lt 14 ]]; then
    echo 'Old macOS, using llvm from brew.'
    llvm_bin_dir="$(brew --prefix llvm)/bin"
    export CC="$llvm_bin_dir/clang" CXX="$llvm_bin_dir/clang++"
  fi

  CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

  echo "Installing Conan packages."
  # Set macOS version to prevent building things for different versions
  # We set build_type for build requirements as well (with :a), because macdeployqt copies its own dylibs,
  #  see https://github.com/conan-io/conan-center-index/issues/22693
  conan install "$PEP_MACOS_ROOT_DIR" \
    --lockfile="$PEP_MACOS_ROOT_DIR/docker-build/builder/conan/conan-ci.lock" \
    --profile:all="$PEP_MACOS_ROOT_DIR/docker-build/builder/conan/conan_profile" \
    --build=missing \
    -s:a os.version=11.0 \
    -s:a build_type="$CMAKE_BUILD_TYPE" \
    -o "&:with_client=True" \
    -o "&:with_servers=False" \
    -o "&:with_castor=False" \
    -o "&:with_tests=False" \
    -o "&:with_benchmark=False" \
    -o "&:custom_build_folder=True" \
    --output-folder="./$BUILD_DIR/"
  if [[ -n "$CLEAN_CONAN" ]]; then
    echo 'Cleaning Conan cache.'
    # Remove old packages
    conan remove '*' --lru 4w --confirm
    # Remove some temporary build files (excludes binaries)
    conan cache clean --build --temp
  fi

  echo "Configuring CMake project."
  MACOS_CMAKE_CONFIGURE_PRESET="conan-$(echo "$CMAKE_BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
  MACOS_CMAKE_BUILD_PRESET=$MACOS_CMAKE_CONFIGURE_PRESET
fi

cmake -S "$PEP_MACOS_ROOT_DIR" --preset "$MACOS_CMAKE_CONFIGURE_PRESET" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DBUILD_GO_SERVERS="$BUILD_GO_SERVERS" \
  -DENABLE_OAUTH_TEST_USERS=OFF

BUILD_TYPE_DIR="${LOCAL_BUILD_TYPE:-.}"

echo "Removing old macOS_artifacts folder."
rm -rf "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts"

echo "Creating new macOS_artifacts folder."
mkdir -p "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/app"
mkdir -p "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/cli"

# Build pepLogon utility.
echo "Building pepLogon utility."
(
  cd "$PEP_MACOS_ROOT_DIR"
  cmake --build --preset "$MACOS_CMAKE_BUILD_PRESET" --target pepLogon
)

# Build PEP Assessor.
echo "Building PEP Assessor."
(
  cd "$PEP_MACOS_ROOT_DIR"
  cmake --build --preset "$MACOS_CMAKE_BUILD_PRESET" --target pepAssessor
)

PEP_MACOS_ASS_APP_NAME="PEP Assessor.app"
PEP_MACOS_ASS_APP_BUILD_ROOT_DIR="$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/app/$PEP_MACOS_ASS_APP_NAME"

# Copy to staging dir and rename app
cp -R "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/cpp/pep/assessor/pepAssessor.app" "$PEP_MACOS_ASS_APP_BUILD_ROOT_DIR"

# Create Info.plist for pepAssessor
(
    cd "$PEP_MACOS_ASS_APP_BUILD_ROOT_DIR/Contents/"
    "$PEP_MACOS_ROOT_DIR/installer/macOS/create-info-plist.sh" "pepAssessor"
)

# The QT deployment tool, macdeployqt, is broken for homebrew, see: https://github.com/orgs/Homebrew/discussions/2823
# A manual replacement of this tool was created in copy-qt-dylibs.sh, but for Conan it is not necessary anymore,
# and it tries to use homebrew Qt libs.
# It would be called like this: "$PEP_MACOS_ROOT_DIR/installer/macOS/copy-qt-dylibs.sh" "$PEP_MACOS_ASS_APP_BUILD_ROOT_DIR/Contents/MacOS/pepAssessor"

# Run macdeployqt on pepAssessor
echo "Running macdeployqt on $PEP_MACOS_ASS_APP_BUILD_ROOT_DIR."

(
  # Apparently conan scripts use unset variables, so temporarily disable check for this
  set +u
  # Add macdeployqt to PATH
  # Suppress shellcheck
  # shellcheck source=/dev/null
  . "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/generators/conanbuild.sh"
  set -u

  # Signing the rest of the app using macdeployqt
  macdeployqt "$PEP_MACOS_ASS_APP_BUILD_ROOT_DIR"
)

# Build pepcli utility.
echo "Building pepcli utility."
(
  cd "$PEP_MACOS_ROOT_DIR"
  cmake --build --preset "$MACOS_CMAKE_BUILD_PRESET" --target pepcli
)

echo "Creating pepcli app."
PEP_MACOS_CLI_APP_NAME="PEP Command Line Interface"
PEP_MACOS_SCRIPTNAME="startterm.sh"
PEP_MACOS_CLI_APP_BUILD_ROOT_DIR="$PEP_MACOS_ROOT_DIR/$BUILD_DIR/macOS_artifacts/cli/$PEP_MACOS_CLI_APP_NAME.app"

mkdir -p "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR"/Contents/{MacOS,Resources,Frameworks}

cp "$PEP_MACOS_ROOT_DIR/cpp/pep/assessor/guilib/appiconcli.icns" "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/Resources/appicon.icns"

cp "$PEP_MACOS_ROOT_DIR/installer/macOS/$PEP_MACOS_SCRIPTNAME" "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/MacOS/"

cp "$PEP_MACOS_ROOT_DIR/installer/macOS/runpepcli.sh" "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/Resources/"

echo "Copying pepcli executables."
cp "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR"/cpp/pep/cli/{pepcli,sparkle} "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/MacOS/"

echo "Copying pepcli frameworks."
cp -R "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/cpp/pep/cli/Frameworks/"* "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/Frameworks/"

if [[ "$CI_COMMIT_REF_NAME" == "Local" ]]; then
  echo "Copying local config."
  cp "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR"/cpp/pep/cli/{ClientConfig.json,rootCA.cert,ShadowAdministration.pub} "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/Resources/"
  cp "$PEP_MACOS_ROOT_DIR/config/local/authserver/AuthserverHTTPSCertificate.pem" "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/Resources/"
fi

echo "Copying peplogon."
cp "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR/cpp/pep/logon/pepLogon" "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/MacOS/"

echo "Copying pepcli autocompletion."
cp -r "$PEP_MACOS_ROOT_DIR/$BUILD_DIR/$BUILD_TYPE_DIR"/autocomplete/{autocomplete_pep.bash,autocomplete_pep.zsh} "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents/Resources/"

# Create Info.plist for pepcli
(
    cd "$PEP_MACOS_CLI_APP_BUILD_ROOT_DIR/Contents"
    "$PEP_MACOS_ROOT_DIR/installer/macOS/create-info-plist.sh" "pepcli"
)

MACOS_SYS_ARCH=$(uname -m)

if [[ "$MACOS_SYS_ARCH" != "arm64" ]] && [[ "$MACOS_SYS_ARCH" != "x86_64" ]]; then
    echo "Error: Unable to retrieve macOS system architecture in $0"
    exit 1
fi

echo "Creating zip file with macOS binaries."

# -c: Create an archive. -k: Create a PKZip archive (.zip)
ditto -c -k --keepParent "$BUILD_DIR/macOS_artifacts" "$BUILD_DIR/macOS_${MACOS_SYS_ARCH}_bins.zip"

# Finished CI job script for macOS installer creation.
echo "Finished CI job script for creation of macOS binaries."