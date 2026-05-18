#!/usr/bin/env bash
set -e

# Moving this file will break ci_cd/macos-ci-build-app-bins.sh

script_dir=$(dirname "$0")

# Get the list of variables before sourcing the file
vars_before=$(compgen -v)

# shellcheck disable=SC1091
# Define the parameters based on the script input value
if [[ "$1" == "PEP Assessor" ]]; then
    source "$script_dir/app-parameters.conf"
elif [[ "$1" == "PEP Command Line Interface" ]]; then
    source "$script_dir/cli-parameters.conf"
elif [[ "$1" == "PEP Download Tool" ]]; then
    source "$script_dir/download-tool-parameters.conf"
elif [[ "$1" == "PEP Upload Tool" ]]; then
    source "$script_dir/upload-tool-parameters.conf"
else
    echo "Error: Invalid script input value. Please provide either 'PEP Assessor', 'PEP Command Line Interface', 'PEP Download Tool' or 'PEP Upload Tool'."
    exit 1
fi

# Get the list of variables after sourcing the file
vars_after=$(compgen -v)

# Use the comm command to get the variables that were added by sourcing the file, and exclude vars_before
new_vars=$(comm -13 <(echo "$vars_before") <(echo "$vars_after") | grep -v '^vars_before$')

# Store the new variables and their values in separate arrays
sourced_var_names=()
sourced_var_values=()
for var in $new_vars; do
    sourced_var_names+=("$var")
    sourced_var_values+=("${!var}")
done

echo "Creating Info.plist for $1."

# Function to conditionally add a key-value pair if the value is not empty
add_key_value() {
    local key="$1"
    local value="$2"
    if [[ -n "$value" ]]; then
        echo "    <key>$key</key>"
        if [[ "$value" == \<* ]]; then
            echo "    $value"
        else
            echo "    <string>$value</string>"
        fi
    fi
}

# Start creating the Info.plist file
cat <<EOF > Info.plist
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
EOF

# Loop over the keys and values arrays
for index in "${!sourced_var_names[@]}"; do
    key="${sourced_var_names[$index]}"
    value="${sourced_var_values[$index]}"
    add_key_value "$key" "$value" >> Info.plist
done

# End the Info.plist file
echo "</dict>" >> Info.plist
echo "</plist>" >> Info.plist

echo "Successfully created Info.plist for $1."

