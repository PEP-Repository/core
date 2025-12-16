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

get_text_input() {
    local prompt="$1"
    local default="$2"
    osascript -e "try" -e "text returned of (display dialog \"$prompt\" default answer \"$default\" buttons {\"OK\", \"Cancel\"} default button \"OK\")" -e "on error number -128" -e "\"\"" -e "end try"
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

cd "$HOME"

"$PEPLOGON_EXECUTABLE"
# Give pepLogon time to start
sleep 2

# Wait for pepLogon process to finish
while pgrep -x "pepLogon" > /dev/null; do
    sleep 1
done

# Get column name
COLUMN=$(get_text_input "Enter the column name to store the file in:" "")
if [ -z "$COLUMN" ]; then
    show_alert "Cancelled" "No column name provided. Operation cancelled."
    exit 0
fi

# Get pseudonym
PSEUDONYM=$(get_text_input "Enter the pseudonym to store the file under:" "")
if [ -z "$PSEUDONYM" ]; then
    show_alert "Cancelled" "No pseudonym provided. Operation cancelled."
    exit 0
fi

# Select file to upload
FILE_PATH=$(osascript -e 'try' -e 'POSIX path of (choose file with prompt "Select the file to upload:")' -e 'on error number -128' -e '""' -e 'end try')

if [ -z "$FILE_PATH" ]; then
    show_alert "Cancelled" "No file selected. Operation cancelled."
    exit 0
fi

if [ ! -f "$FILE_PATH" ]; then
    show_alert "Error" "Selected file does not exist or is not accessible."
    exit 1
fi

# Check if data already exists for this participant and column
LIST_OUTPUT=$("$PEPCLI_EXECUTABLE" list -m -l --no-inline-data -P "$PSEUDONYM" -c "$COLUMN" 2>/dev/null || echo "")

# If output is not empty and not just "[]", data exists
if [ -n "$LIST_OUTPUT" ] && [ "$LIST_OUTPUT" != "[]" ]; then
    osascript -e "display dialog \"Data already exists in column '$COLUMN' for pseudonym '$PSEUDONYM'. Do you want to overwrite it?\" buttons {\"Cancel\", \"Overwrite\"} default button \"Cancel\"" > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        show_alert "Cancelled" "Upload cancelled by user."
        exit 0
    fi
fi

# Upload the file using pepcli
ERROR_OUTPUT=$("$PEPCLI_EXECUTABLE" store --column "$COLUMN" --participant "$PSEUDONYM" --file "$FILE_PATH" 2>&1)
if [ $? -eq 0 ]; then
    show_alert "Success" "File uploaded successfully to column '$COLUMN' for pseudonym '$PSEUDONYM'."
else
    exit_code=$?
    show_alert "Error" "pepcli store command failed with exit code $exit_code:\\n\\n$ERROR_OUTPUT"
    exit $exit_code
fi
