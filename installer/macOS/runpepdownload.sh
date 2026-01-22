#!/usr/bin/env bash

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

show_dialog() {
    local message="$1"
    local button1="$2"
    local button2="$3"
    local default_button="$4"
    local result
    result=$(osascript 2>&1 <<EOF
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
    
    if echo "$error_output" | grep -q "Access denied"; then
        printf "You do not have permission to download data.\\n\\nPlease contact your administrator if you believe you should have access."
    elif echo "$error_output" | grep -q -e "Not enrolled" -e "certificate expired" -e "certificate chain not trusted" -e "Invalid signature"; then
        if [ -f "$HOME/Downloads/ClientKeys.json" ]; then
            rm "$HOME/Downloads/ClientKeys.json"
        fi
        printf "Authentication failed. Your session may have expired or your certificate is not trusted.\\n\\nPlease run the application again to re-authenticate."
    else
        printf "%s" "$error_output"
    fi
}

run_peplogon() {
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
    if show_dialog "Updates are available. Do you want to install them?" "No" "Yes" "Yes"; then
        "$SPARKLE_EXECUTABLE" "$PEP_APP_DIR" --check-immediately --interactive
        show_notification "PEP Update" "Updates installed successfully."
    else
        "$SPARKLE_EXECUTABLE" "$PEP_APP_DIR" --check-immediately --defer-install
        show_notification "PEP Update" "Update installation cancelled."
    fi
fi

cd "$HOME/Downloads" || {
    show_alert "Error" "Could not change to downloads directory."
    exit 1
}

# Only run pepLogon if ClientKeys.json doesn't exist
if [ ! -f "$HOME/Downloads/ClientKeys.json" ]; then
    run_peplogon
fi

OUTPUT_DIR=$(osascript -e 'try' -e 'POSIX path of (choose folder with prompt "Select the directory to download files to:")' -e 'on error number -128' -e '""' -e 'end try')

if [ -z "$OUTPUT_DIR" ]; then
    show_alert "Cancelled" "No output directory selected. Operation cancelled."
    exit 0
fi

if [ -d "$OUTPUT_DIR/pulled-data" ]; then
    show_alert "Error" "Folder pulled-data in selected directory already exists. Please remove it or choose a different directory."
    exit 1
fi

PULL_OUTPUT=$("$PEPCLI_EXECUTABLE" pull --force --all-accessible --export csv --output-directory "$OUTPUT_DIR/pulled-data" 2>&1)
PULL_EXIT=$?

if [ $PULL_EXIT -eq 0 ]; then
    show_alert "Success" "Files downloaded successfully to '$OUTPUT_DIR'."
    open "$OUTPUT_DIR"
    exit 0
else
    ERROR_MSG=$(parse_pep_error "$PULL_OUTPUT")

    show_alert "Error" "Download failed:\\n\\n$ERROR_MSG"
    exit 1
fi