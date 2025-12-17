#!/usr/bin/env bash
# shellcheck disable=all
set -x

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
    osascript > /dev/null <<EOF
display alert "$title" message "$message" buttons {"OK"} default button "OK"
EOF
}

show_notification() {
    local title="$1"
    local message="$2"
    osascript <<EOF
display notification "$message" with title "$title"
EOF
}

get_text_input() {
    local prompt="$1"
    local default="$2"
    osascript -e "try" -e "text returned of (display dialog \"$prompt\" default answer \"$default\" buttons {\"OK\", \"Cancel\"} default button \"OK\")" -e "on error number -128" -e "\"\"" -e "end try"
}

show_dialog() {
    local message="$1"
    local button1="$2"
    local button2="$3"
    local default_button="$4"
    
    local result=$(osascript 2>&1 <<EOF
display dialog "$message" buttons {"$button1", "$button2"} default button "$default_button"
EOF
)
    
    # Check if user clicked button1 (returns "button returned:button1")
    if echo "$result" | grep -q "button returned:$button1"; then
        return 1
    else
        return 0
    fi
}

parse_pep_error() {
    local error_output="$1"
    local operation="$2"
    
    if echo "$error_output" | grep -q "Unknown column specified"; then
        echo "Could not find column '$COLUMN' in PEP.\\n\\nPlease check the column name and try again."
    elif echo "$error_output" | grep -q "Access denied"; then
        if [ "$operation" = "list" ]; then
            echo "You do not have permission to check data existence in column '$COLUMN'.\\n\\nPlease contact your administrator if you believe you should have access."
        else
            echo "You do not have permission to upload data to column '$COLUMN'.\\n\\nPlease contact your administrator if you believe you should have access."
        fi
    elif echo "$error_output" | grep -q -e "Not enrolled or certificate expired"; then
        show_alert "Authentication Required" "Your session has expired. Please log in again."
        run_peplogon > /dev/null 2>&1
        echo "REAUTHENTICATED"
    else
        echo "$error_output"
    fi
}

run_peplogon() {
    local reauth="$1"
    "$PEPLOGON_EXECUTABLE"
    PEPLOGON_EXIT=$?
    
    if [ $PEPLOGON_EXIT -ne 0 ]; then
        show_alert "Login Failed" "Authentication failed. Please try again."
        return 1
    fi
    
    # Give pepLogon time to start
    sleep 2

    # Wait for pepLogon process to finish
    while pgrep -x "pepLogon" > /dev/null; do
        sleep 1
    done
    
    return 0
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

# Only run pepLogon if ClientKeys.json doesn't exist
if [ ! -f "$HOME/ClientKeys.json" ]; then
    run_peplogon
fi

upload_file() {
    # Get column name
    COLUMN=$(get_text_input "Enter the column name to store the file in:" "")
    if [ -z "$COLUMN" ]; then
        show_alert "Cancelled" "No column name provided. Operation cancelled."
        return 1
    fi

    # Get pseudonym
    PSEUDONYM=$(get_text_input "Enter the pseudonym to store the file under:" "")
    if [ -z "$PSEUDONYM" ]; then
        show_alert "Cancelled" "No pseudonym provided. Operation cancelled."
        return 1
    fi

    # Check if data already exists for this participant and column
    LIST_OUTPUT=$("$PEPCLI_EXECUTABLE" list -m -l --no-inline-data -p "$PSEUDONYM" -c "$COLUMN" 2>&1)
    LIST_EXIT=$?

    if [ $LIST_EXIT -ne 0 ]; then
        ERROR_MSG=$(parse_pep_error "$LIST_OUTPUT" "list")
        if [ "$ERROR_MSG" = "REAUTHENTICATED" ]; then
            return 2
        fi
        show_alert "Error" "$ERROR_MSG"
        return 1
    fi

    # Check if data exists by looking for "Listed 0 results"
    if ! echo "$LIST_OUTPUT" | grep -q "Listed 0 results"; then
        if ! show_dialog "Data already exists in column '$COLUMN' for pseudonym '$PSEUDONYM'. Do you want to overwrite it?" "Cancel" "Overwrite" "Cancel"; then
            show_alert "Cancelled" "Upload cancelled by user."
            return 1
        fi
    fi

    # Select file to upload
    FILE_PATH=$(osascript -e 'try' -e 'POSIX path of (choose file with prompt "Select the file to upload:")' -e 'on error number -128' -e '""' -e 'end try')

    if [ -z "$FILE_PATH" ]; then
        show_alert "Cancelled" "No file selected. Operation cancelled."
        return 1
    fi

    if [ ! -f "$FILE_PATH" ]; then
        show_alert "Error" "Selected file does not exist or is not accessible."
        return 1
    fi

    # Upload the file using pepcli
    STORE_OUTPUT=$("$PEPCLI_EXECUTABLE" store --column "$COLUMN" --participant "$PSEUDONYM" --input-path "$FILE_PATH" 2>&1)
    STORE_EXIT=$?

    if [ $STORE_EXIT -eq 0 ]; then
        show_alert "Success" "File uploaded successfully to column '$COLUMN' for pseudonym '$PSEUDONYM'."
        return 0
    else
        ERROR_MSG=$(parse_pep_error "$STORE_OUTPUT" "store")
        if [ "$ERROR_MSG" = "REAUTHENTICATED" ]; then
            return 2
        fi
        show_alert "Error" "Upload failed:\\n\\n$ERROR_MSG"
        return 1
    fi
}

# Main upload loop
while true; do
    upload_file
    UPLOAD_RESULT=$?
    
    # If return code is 2 we just reauthenticated, so we dont need continue dialog
    if [ $UPLOAD_RESULT -eq 2 ]; then
        continue
    fi
    
    # Ask user if they want to continue
    if show_dialog "Do you want to continue uploading?" "No" "Yes" "Yes"; then
        continue
    else
        exit 0
    fi
done
