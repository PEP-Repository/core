#!/usr/bin/env bash
set -e

# Copy Qt dylibs from Homebrew

# Moving this file will break ci_cd/macos-ci-build-app-bins.sh

# Check for -v argument, which enables verbose output
VERBOSE=0
while getopts "v" opt; do
  case ${opt} in
    v)
      VERBOSE=1
      ;;
    \?)
      echo "Invalid option: -$OPTARG" 1>&2
      exit 1
      ;;
  esac
done
shift $((OPTIND -1))

dylib_log() {
    local level="$1"
    local message="$2"

    if [[ "$level" == "error" ]]; then
        echo "Error: $message" >&2
        exit 1
    elif [[ "$VERBOSE" -eq 1 ]]; then
        if [[ "$level" == "log" ]]; then
            echo "Log: $message"
        elif [[ "$level" == "warning" ]]; then
            echo "Warning: $message"
        else
            echo "Wrong input to logging function."
            exit 1
        fi
    elif [[ "$VERBOSE" -eq 0 ]]; then
        if [[ ("$level" == "log") || ("$level" == "warning") ]]; then
            # Do nothing
            :
        else
            echo "Wrong input to logging function."
            exit 1
        fi    
    fi
}

# Define the target executable path and the target frameworks directory
if [[ -z "$1" ]]; then
    dylib_log "error" "No argument supplied for the target binary name"
fi

TARGET_EXECUTABLE_PATH=$1
MACOS_APP_DIR="$(readlink -f "$(dirname "$TARGET_EXECUTABLE_PATH")"/../..)"
FRAMEWORKS_DIR="$MACOS_APP_DIR/Contents/Frameworks"

# Define the directory containing the Qt frameworks
if [[ -z "$HOMEBREW_PREFIX" ]]; then
    HOMEBREW_PREFIX=$(readlink -f "$(dirname "$(which brew)")/../")
    if [[ -z "$HOMEBREW_PREFIX" ]]; then
        dylib_log "error" "Failed to determine HOMEBREW_PREFIX."
    fi
fi
QT_FRAMEWORKS_DIR="$HOMEBREW_PREFIX/opt/qt/lib"

# Create the target frameworks directory if it doesn't exist
mkdir -p "$FRAMEWORKS_DIR"

# Set global for logging recursivity depth
RECURSIVE_DEPTH=0

check_all_dependencies() {
    local DEPENDENCIES_CHECK
    DEPENDENCIES_CHECK=$(otool -L "$1" | tail -n +2)
    while read -r DEPENDENCY_CHECK; do
        if dependency_is_not_in_binary_path "$DEPENDENCY_CHECK"; then
            dylib_log "log" "Found incorrect dependency: $DEPENDENCY_CHECK"
            return 1
        fi
    done <<< "$DEPENDENCIES_CHECK"
    return 0
}

# Function to check if a dependency is a Qt framework and not in the executable path
dependency_is_not_in_binary_path() {
    local dependency="$1"
    if [[ "$dependency" == "@rpath"* || "$dependency" == "$HOMEBREW_PREFIX"* ]]; then
        return 0
    else
        return 1
    fi
}

dependency_is_qt_framework() {
    local dependency="$1"
    if [[ "$dependency" == *Qt*.framework* ]] || [[ "$dependency" == *Sparkle.framework* ]]; then
        return 0  # Qt Framework
    elif [[ "$dependency" == *.dylib ]]; then
        return 1  # Dynamic Library
    else
        dylib_log "error" "Failed to determine dependency type of $dependency."
    fi
}

copy_framework() {
    local FRAMEWORK_ORIGIN_DIR="$1"
    local FRAMEWORK_TARGET_PATH="$2"
    local TARGET_DIR="$3"
    local FULL_FRAMEWORK_NAME="$4"
    local USE_SYMLINK_FLAG="$5"

    # Check if the target doesn't already exist
    if [[ ! -e "$FRAMEWORK_TARGET_PATH" ]]; then
        if [[ -e "$FRAMEWORK_ORIGIN_DIR" ]]; then
            # Copy the framework to the target directory
            if [[ "$USE_SYMLINK_FLAG" -eq 1 ]]; then
                CP_COMMAND="cp -LR"
            else
                CP_COMMAND="cp -R"
            fi

            if $CP_COMMAND "$FRAMEWORK_ORIGIN_DIR" "$TARGET_DIR"; then
                dylib_log "log" "Successfully copied $FULL_FRAMEWORK_NAME to $TARGET_DIR."
            else
                dylib_log "error" "Failed to copy $FULL_FRAMEWORK_NAME to $TARGET_DIR."
            fi
        else
            dylib_log "warning" "Framework origin directory $FRAMEWORK_ORIGIN_DIR does not exist."
        fi
    else
        dylib_log "log" "Framework $FULL_FRAMEWORK_NAME already exists in $TARGET_DIR."
    fi
}

link_qt() {
    local TARGET_BINARY="$1"
    local CONTAINER_ORIGIN="$2"

    # Check if the target binary exists
    if [[ ! -f "$TARGET_BINARY" ]]; then
        dylib_log "error" "Target binary $TARGET_BINARY does not exist."
    fi

    # Check if the Qt frameworks directory exists
    if [[ ! -d "$QT_FRAMEWORKS_DIR" ]]; then
        dylib_log "error" "Qt frameworks directory $QT_FRAMEWORKS_DIR does not exist."
    fi

    # Check if the target frameworks directory exists
    if [[ ! -d "$FRAMEWORKS_DIR" ]]; then
        dylib_log "error" "Target frameworks directory $FRAMEWORKS_DIR does not exist."
    fi

    if check_all_dependencies "$TARGET_BINARY"; then
        dylib_log "log" "Linking $TARGET_BINARY Successful"
        RECURSIVE_DEPTH=$((RECURSIVE_DEPTH-1))
        dylib_log "log" "!!!!!!!!!RECURSIVE DEPTH: $RECURSIVE_DEPTH !!!!!!!!!"
        return 0
    fi

    # List the dependencies of the target binary
    local DEPENDENCIES
    DEPENDENCIES=$(otool -L "$TARGET_BINARY" | tail -n +2)
    local TARGET_BINARY_NAME
    TARGET_BINARY_NAME=$(echo "$TARGET_BINARY" | awk -F '/' '{print $NF}')

    # Loop over the dependencies
    while read -r DEPENDENCY; do

        if dependency_is_not_in_binary_path "$DEPENDENCY"; then

            local OLD_FRAMEWORK_PATH
            OLD_FRAMEWORK_PATH=$(echo "$DEPENDENCY" | awk -F '(' '{print $1}' | xargs)
            local FRAMEWORK_NAME
            FRAMEWORK_NAME=$(echo "$OLD_FRAMEWORK_PATH" | awk -F '/' '{print $NF}')

            dylib_log "log" "Checking dependency: $FRAMEWORK_NAME, in $TARGET_BINARY_NAME"

            # Check if the dependency is a Qt framework and not already correctly linked
            if dependency_is_qt_framework "$OLD_FRAMEWORK_PATH"; then
                local FULL_FRAMEWORK_NAME="$FRAMEWORK_NAME.framework/$FRAMEWORK_NAME"
                local NEW_FRAMEWORK_PATH="@executable_path/../Frameworks/$FULL_FRAMEWORK_NAME"
                local FRAMEWORK_DIRNAME=$FRAMEWORK_NAME.framework
                local FRAMEWORK_ORIGIN_DIR="$QT_FRAMEWORKS_DIR/$FRAMEWORK_DIRNAME"
                local FRAMEWORK_TARGET_PATH="$FRAMEWORKS_DIR/$FRAMEWORK_DIRNAME"

                copy_framework "$FRAMEWORK_ORIGIN_DIR" "$FRAMEWORK_TARGET_PATH" "$FRAMEWORKS_DIR" "$FULL_FRAMEWORK_NAME" 0
            else
                # CATCH @rpath + /homebrew/ condition somewhere
                local FULL_FRAMEWORK_NAME=$FRAMEWORK_NAME
                local NEW_FRAMEWORK_PATH="@executable_path/../Frameworks/$FULL_FRAMEWORK_NAME"
                local FRAMEWORK_ORIGIN_DIR=$OLD_FRAMEWORK_PATH
                local FRAMEWORK_TARGET_PATH="$FRAMEWORKS_DIR/$FULL_FRAMEWORK_NAME"
                if [[ "$FRAMEWORK_ORIGIN_DIR" == @rpath* ]]; then
                    dylib_log "log" "Trying to resolve @rpath in $FRAMEWORK_ORIGIN_DIR to parent directory $CONTAINER_ORIGIN."
                    FRAMEWORK_ORIGIN_DIR="${FRAMEWORK_ORIGIN_DIR/#@rpath/$CONTAINER_ORIGIN}"
                fi

                copy_framework "$FRAMEWORK_ORIGIN_DIR" "$FRAMEWORK_TARGET_PATH" "$FRAMEWORKS_DIR" "$FULL_FRAMEWORK_NAME" 1
            fi

            # Use install_name_tool to change the install name in the target binary -change old new file
            # and change the id in the framework binary -id name file
            if [[ "$VERBOSE" -eq 1 ]]; then
                if /usr/bin/install_name_tool -change "$OLD_FRAMEWORK_PATH" "$NEW_FRAMEWORK_PATH" "$TARGET_BINARY"; then
                    dylib_log "log" "Successfully updated install name of $FULL_FRAMEWORK_NAME in $TARGET_BINARY from $OLD_FRAMEWORK_PATH to $NEW_FRAMEWORK_PATH."
                else
                    dylib_log "error" "Failed to update install name of $FULL_FRAMEWORK_NAME in $TARGET_BINARY."
                fi
                if /usr/bin/install_name_tool -id "$NEW_FRAMEWORK_PATH" "$FRAMEWORKS_DIR/$FULL_FRAMEWORK_NAME"; then
                    dylib_log "log" "Successfully updated id of $FULL_FRAMEWORK_NAME in $FRAMEWORKS_DIR/$FULL_FRAMEWORK_NAME."
                else
                    dylib_log "error" "Failed to update id of $FULL_FRAMEWORK_NAME in $FRAMEWORKS_DIR/$FULL_FRAMEWORK_NAME."
                fi
            else
                OUTPUT=$(/usr/bin/install_name_tool -change "$OLD_FRAMEWORK_PATH" "$NEW_FRAMEWORK_PATH" "$TARGET_BINARY" 2>&1) && installnameresult=$? || installnameresult=$?
                if [[ "$installnameresult" -eq 0 ]]; then
                    dylib_log "log" "Successfully updated install name of $FULL_FRAMEWORK_NAME in $TARGET_BINARY from $OLD_FRAMEWORK_PATH to $NEW_FRAMEWORK_PATH."
                else
                    dylib_log "error" "Failed to update install name of $FULL_FRAMEWORK_NAME in $TARGET_BINARY. Output was: $OUTPUT"
                fi
                OUTPUT=$(/usr/bin/install_name_tool -id "$NEW_FRAMEWORK_PATH" "$FRAMEWORKS_DIR/$FULL_FRAMEWORK_NAME" 2>&1) && installnameresult=$? || installnameresult=$?
                if [[ "$installnameresult" -eq 0 ]]; then
                    dylib_log "log" "Successfully updated id of $FULL_FRAMEWORK_NAME in $FRAMEWORKS_DIR/$FULL_FRAMEWORK_NAME."
                else
                    dylib_log "error" "Failed to update id of $FULL_FRAMEWORK_NAME in $FRAMEWORKS_DIR/$FULL_FRAMEWORK_NAME. Output was: $OUTPUT"
                fi
            fi

            # Recursively update the dependencies of the framework binary, if the framework binary is not already the target binary
            if [[ "$FRAMEWORK_NAME" != "$TARGET_BINARY_NAME" ]]; then
                RECURSIVE_DEPTH=$((RECURSIVE_DEPTH+1))
                dylib_log "log" "!!!!!!!!!RECURSIVE DEPTH: $RECURSIVE_DEPTH !!!!!!!!!"
                link_qt "$FRAMEWORKS_DIR/$FULL_FRAMEWORK_NAME" "$(dirname "$FRAMEWORK_ORIGIN_DIR")"
            else
                dylib_log "log" "Skipping recursive linking of $FRAMEWORK_NAME in $TARGET_BINARY_NAME."
            fi

        fi
    done <<< "$DEPENDENCIES"
    RECURSIVE_DEPTH="$((RECURSIVE_DEPTH-1))"
    dylib_log "log" "!!!!!!!!!RECURSIVE DEPTH: $RECURSIVE_DEPTH !!!!!!!!!"
    return 0
}

link_qt_plugins() {
    PLUGINS_DIR="$MACOS_APP_DIR/Contents/PlugIns"

    # Create the target frameworks directory if it doesn't exist
    mkdir -p "$PLUGINS_DIR"

    QT_MODULES_DIR="$QT_FRAMEWORKS_DIR/../share/qt/modules"
    QT_PLUGINS_DIR="$QT_FRAMEWORKS_DIR/../share/qt/plugins"
    #PLUGIN_LIST="accessiblebridge;platforms;platforms_darwin;xcbglintegrations;platformthemes;platforminputcontexts;generic;iconengines;imageformats;egldeviceintegrations;networkaccess;networkinformation;tls;printsupport;styles"

    # Loop over the frameworks in FRAMEWORKS_DIR
    for framework_path in "$FRAMEWORKS_DIR"/*.framework; do
        framework_name=$(basename "$framework_path" .framework | sed 's/^Qt//')

        # Open the framework_name.json file in QT_MODULES_DIR
        json_file="$QT_MODULES_DIR/$framework_name.json"
        if [[ -f "$json_file" ]]; then
            # Check if 'plugin_types' is in the JSON file
            if grep -q '"plugin_types":' "$json_file"; then
                # Get the list of plugins from the "plugin_types" field
                plugin_types=$(python3 -c "import json; print('\n'.join(json.load(open('$json_file'))['plugin_types']))")

                # Loop over the plugin types
                for plugin_type in $plugin_types; do
                    # Copy the framework for each plugin type
                    if ! [[ -e "$PLUGINS_DIR/$plugin_type" ]]; then
                        if [[ -e "$QT_PLUGINS_DIR/$plugin_type" ]]; then
                            copy_framework "$QT_PLUGINS_DIR/$plugin_type" "$PLUGINS_DIR/$plugin_type" "$PLUGINS_DIR" "$plugin_type" 1

                            for dylib_file in "$PLUGINS_DIR/$plugin_type"/*.dylib; do
                                # Reset global for logging recursivity depth
                                RECURSIVE_DEPTH=0
                                link_qt "$dylib_file" "$PLUGINS_DIR/$plugin_type"
                            done
                        else
                            dylib_log "warning" "Plugin origin directory $QT_PLUGINS_DIR/$plugin_type does not exist."
                        fi
                    else
                        dylib_log "warning" "Plugin target $plugin_type already exists."
                    fi
                done
            else
                dylib_log "warning" "plugin_types field not found in $json_file."
            fi
        else
            dylib_log "warning" "Failed to find $framework_name.json in $QT_MODULES_DIR."
        fi
    done
    return 0
}

echo "Linking core qt dylibs in $TARGET_EXECUTABLE_PATH"

if link_qt "$TARGET_EXECUTABLE_PATH" "$QT_FRAMEWORKS_DIR"; then
    echo "Successfully linked core qt dylibs in $TARGET_EXECUTABLE_PATH"
else
    dylib_log "error" "Failed to link core qt dylibs in $TARGET_EXECUTABLE_PATH"
fi

echo "Linking qt plugins in $TARGET_EXECUTABLE_PATH"

if link_qt_plugins; then
    echo "Successfully linked qt plugins in $TARGET_EXECUTABLE_PATH"
else
    dylib_log "error" "Failed to link qt plugins in $TARGET_EXECUTABLE_PATH"
fi