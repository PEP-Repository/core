#!/usr/bin/env bash
# shellcheck disable=all

set -e

PEP_SCRIPT_DIR_REL="$(dirname -- "$0")"
PEP_SCRIPT_DIR=$(cd "$PEP_SCRIPT_DIR_REL" && pwd)
PEP_EXECUTABLE_DIR_REL="$PEP_SCRIPT_DIR/../MacOS"
PEP_EXECUTABLE_DIR=$(cd "$PEP_EXECUTABLE_DIR_REL" && pwd)
PEP_APP_DIR_REL="$PEP_EXECUTABLE_DIR/../.."
PEP_APP_DIR=$(cd "$PEP_APP_DIR_REL" && pwd)

SPARKLE_EXECUTABLE="$PEP_EXECUTABLE_DIR/sparkle"
PEPLOGON_EXECUTABLE="$PEP_EXECUTABLE_DIR/pepLogon"
PEPCLI_EXECUTABLE="$PEP_EXECUTABLE_DIR/pepcli"

show_alert() {
    local title="$1"
    local message="$2"
    osascript -e "display alert \"$title\" message \"$message\" buttons {\"OK\"} default button \"OK\"" > /dev/null
}

show_notification() {
    local title="$1"
    local message="$2"
    osascript -e "display notification \"$message\" with title \"$title\""
}

# Use Sparkle to check for updates
if "$SPARKLE_EXECUTABLE" "$PEP_APP_DIR" --probe; then
    # Ask the user if they want to install the updates
    osascript -e 'tell app "System Events" to display dialog "Updates are available. Do you want to install them?" buttons {"Yes", "No"} default button "Yes"' > /dev/null
    if [ $? -eq 0 ]; then
        "$SPARKLE_EXECUTABLE" "$PEP_APP_DIR" --check-immediately --interactive
        show_notification "PEP Update" "Updates installed successfully."
    else
        "$SPARKLE_EXECUTABLE" "$PEP_APP_DIR" --check-immediately --defer-install
        show_notification "PEP Update" "Update installation cancelled."
    fi
fi

cd "$HOME/Downloads"

"$PEPLOGON_EXECUTABLE"
# Give pepLogon time to start
sleep 2

# Wait for pepLogon process to finish
while pgrep -x "pepLogon" > /dev/null; do
    sleep 1
done

OUTPUT_DIR=$(osascript -e 'try' -e 'POSIX path of (choose folder with prompt "Select the directory to download files to:")' -e 'on error number -128' -e '""' -e 'end try')

if [ -z "$OUTPUT_DIR" ]; then
    show_alert "Cancelled" "No output directory selected. Operation cancelled."
    exit 0
fi

if [ -d "$OUTPUT_DIR/pulled-data" ]; then
    show_alert "Error" "Folder pulled-data in selected directory already exists. Please remove it or choose a different directory."
    exit 1
fi

if "$PEPCLI_EXECUTABLE" pull --force --all-accessible --export csv --output-directory "$OUTPUT_DIR/pulled-data"; then
    show_alert "Success" "Files downloaded successfully to '$OUTPUT_DIR'."
else
    exit_code=$?
    show_alert "Error" "pepcli pull command failed with exit code $exit_code. Check logs if available."
    exit $exit_code
fi