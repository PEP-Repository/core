#!/usr/bin/env bash
# shellcheck disable=SC2207
# As mapfile is not available on macos
set -euo pipefail

# Assumed to be ran in a folder with the arm64 and x86_64 subdirectories, each containing their corresponding macOS_artifacts, with app, cli, dt and ut subdirectories, each containing their corresponding .app bundles
# Moving this file will break ci_cd/macos-ci-universal-pkg

echo "Starting CI job script for macOS universal binary creation."

IFS=$'\n' app_files_x86=($(find x86_64/macOS_artifacts/assessor_app -maxdepth 1 -name "*.app"))
IFS=$'\n' app_files_arm=($(find arm64/macOS_artifacts/assessor_app -maxdepth 1 -name "*.app"))
IFS=$'\n' cli_files_x86=($(find x86_64/macOS_artifacts/cli_app -maxdepth 1 -name "*.app"))
IFS=$'\n' cli_files_arm=($(find arm64/macOS_artifacts/cli_app -maxdepth 1 -name "*.app"))
IFS=$'\n' dt_files_x86=($(find x86_64/macOS_artifacts/download_tool_app -maxdepth 1 -name "*.app"))
IFS=$'\n' dt_files_arm=($(find arm64/macOS_artifacts/download_tool_app -maxdepth 1 -name "*.app"))
IFS=$'\n' ut_files_x86=($(find x86_64/macOS_artifacts/upload_tool_app -maxdepth 1 -name "*.app"))
IFS=$'\n' ut_files_arm=($(find arm64/macOS_artifacts/upload_tool_app -maxdepth 1 -name "*.app"))

# Check that exactly one .app file was found in each directory
if [[ ${#app_files_x86[@]} -ne 1 ]] || [[ ${#app_files_arm[@]} -ne 1 ]] || [[ ${#cli_files_x86[@]} -ne 1 ]] || [[ ${#cli_files_arm[@]} -ne 1 ]] || [[ ${#dt_files_x86[@]} -ne 1 ]] || [[ ${#dt_files_arm[@]} -ne 1 ]] || [[ ${#ut_files_x86[@]} -ne 1 ]] || [[ ${#ut_files_arm[@]} -ne 1 ]]; then
    echo "Error: Expected exactly one .app file in each directory, but found a different number"
    exit 1
fi

# Check that the .app filenames are the same for the x86 and arm versions
if [[ "$(basename "${app_files_x86[0]}")" != "$(basename "${app_files_arm[0]}")" ]] || [[ "$(basename "${cli_files_x86[0]}")" != "$(basename "${cli_files_arm[0]}")" ]] || [[ "$(basename "${dt_files_x86[0]}")" != "$(basename "${dt_files_arm[0]}")" ]] || [[ "$(basename "${ut_files_x86[0]}")" != "$(basename "${ut_files_arm[0]}")" ]]; then
    echo "Error: The .app filenames do not match"
    exit 1
else
    PEP_MACOS_ASSESSOR_APP_NAME="$(basename "${app_files_x86[0]%.app}")"
    PEP_MACOS_CLI_APP_NAME="$(basename "${cli_files_x86[0]%.app}")"
    PEP_MACOS_DOWNLOAD_TOOL_APP_NAME="$(basename "${dt_files_x86[0]%.app}")"
    PEP_MACOS_UPLOAD_TOOL_APP_NAME="$(basename "${ut_files_x86[0]%.app}")"
fi

# Provide the base x86 apps
mkdir -p "universal/macOS_artifacts/"{app,cli,dt,ut}

cp -RP "x86_64/macOS_artifacts/assessor_app/" "universal/macOS_artifacts/assessor_app/"
cp -RP "x86_64/macOS_artifacts/cli_app/" "universal/macOS_artifacts/cli_app/"
cp -RP "x86_64/macOS_artifacts/download_tool_app/" "universal/macOS_artifacts/download_tool_app/"
cp -RP "x86_64/macOS_artifacts/upload_tool_app/" "universal/macOS_artifacts/upload_tool_app/"

lipo_app_files() {
    local app_dir1="$1"
    local app_dir2="$2"
    local target_dir="$3"

    # Check if app directories exist
    if [[ ! -d "$app_dir1" ]]; then
        echo "Error: App directory '$app_dir1' does not exist."
        exit 1
    fi

    if [[ ! -d "$app_dir2" ]]; then
        echo "Error: App directory '$app_dir2' does not exist."
        exit 1
    fi

    # Check if target directory exists
    if [[ ! -d "$target_dir" ]]; then
        echo "Error: Target directory '$target_dir' does not exist."
        exit 1
    fi

    # Merge dylibs using command substitution to pass the correct file paths
    merge_dylibs() {
        local file_path1="$1"
        local app_dir1="$2"
        local app_dir2="$3"
        local target_dir="$4"
        local file_path2="${file_path1/#$app_dir1/$app_dir2}"
        local file_path3="${file_path1/#$app_dir1/$target_dir}"
        lipo -create "$file_path1" "$file_path2" -output "$file_path3"
    }

    export -f merge_dylibs

    # Search for executable files in the MacOS directory, execute a merge function using bash command strings, in this case $0 refers to the first argument passed
    for dir in "$app_dir1/Contents/Plugins" "$app_dir1/Contents/Resources" "$app_dir1/Contents/MacOS"; do
        if [[ -d "$dir" ]]; then
            find "$dir" -type f -exec bash -c 'file "$0" | grep -q "Mach-O" && ! file "$0" | grep -q "universal binary"' {} \; -exec bash -c 'merge_dylibs "$0" "$1" "$2" "$3"' {} "$app_dir1" "$app_dir2" "$target_dir" \;
        fi
    done

    # The depth is set to 1 to avoid merging any frameworks themselves, as they need some manual consideration
    if [[ -d "$app_dir1/Contents/Frameworks" ]]; then
        find "$app_dir1/Contents/Frameworks" -maxdepth 1 -type f -exec bash -c 'file "$0" | grep -q "Mach-O" && ! file "$0" | grep -q "universal binary"' {} \; -exec bash -c 'merge_dylibs "$0" "$1" "$2" "$3"' {} "$app_dir1" "$app_dir2" "$target_dir" \;
    fi
  }

# Call the function with the app directories, for PEPAssessor
lipo_app_files "x86_64/macOS_artifacts/assessor_app/$PEP_MACOS_ASSESSOR_APP_NAME.app" "arm64/macOS_artifacts/assessor_app/$PEP_MACOS_ASSESSOR_APP_NAME.app" "universal/macOS_artifacts/assessor_app/$PEP_MACOS_ASSESSOR_APP_NAME.app"

# And for PEP CLI
lipo_app_files "x86_64/macOS_artifacts/cli_app/$PEP_MACOS_CLI_APP_NAME.app" "arm64/macOS_artifacts/cli_app/$PEP_MACOS_CLI_APP_NAME.app" "universal/macOS_artifacts/cli_app/$PEP_MACOS_CLI_APP_NAME.app"

# And for PEP Download Tool
lipo_app_files "x86_64/macOS_artifacts/download_tool_app/$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME.app" "arm64/macOS_artifacts/download_tool_app/$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME.app" "universal/macOS_artifacts/download_tool_app/$PEP_MACOS_DOWNLOAD_TOOL_APP_NAME.app"

# And for PEP Upload Tool
lipo_app_files "x86_64/macOS_artifacts/upload_tool_app/$PEP_MACOS_UPLOAD_TOOL_APP_NAME.app" "arm64/macOS_artifacts/upload_tool_app/$PEP_MACOS_UPLOAD_TOOL_APP_NAME.app" "universal/macOS_artifacts/upload_tool_app/$PEP_MACOS_UPLOAD_TOOL_APP_NAME.app"