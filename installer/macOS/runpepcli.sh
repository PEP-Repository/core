#!/usr/bin/env zsh
# shellcheck disable=all

echo -ne "\033c"
echo -ne "\033]0;PEP Command Line Interface\007"

PEP_SCRIPT_DIR_REL="$(dirname -- "$0")"
PEP_SCRIPT_DIR=$PEP_SCRIPT_DIR_REL:P
PEP_EXECUTABLE_DIR_REL="$PEP_SCRIPT_DIR/../MacOS"
PEP_EXECUTABLE_DIR=$PEP_EXECUTABLE_DIR_REL:P
PEP_APP_DIR_REL="$PEP_EXECUTABLE_DIR/../.."
PEP_APP_DIR=$PEP_APP_DIR_REL:P

# Use Sparkle to probe for updates
echo "Checking for updates..."

if "$PEP_EXECUTABLE_DIR/sparkle" "$PEP_APP_DIR" --probe; then
    # Ask the user if they want to install the updates
    echo -n "Updates are available. Do you want to install them? (y/n): "
    read -r install_updates
    if [[ $install_updates == [Yy]* ]]; then
        # Use Sparkle to install the updates
        "$PEP_EXECUTABLE_DIR/sparkle" "$PEP_APP_DIR" --check-immediately --verbose
    fi
else
    echo "PEP Command Line Interface is up to date."
fi

cd ~/Downloads || { echo "Failed to change directory to ~/Downloads."; exit 1; }
export PATH=$PATH:"$PEP_EXECUTABLE_DIR"

# Ask for pepLogon
while true; do
    echo -n "Would you like to use pepLogon? (y/n): "
    read -r choice
    case $choice in
        [Yy]* ) "$PEP_EXECUTABLE_DIR/pepLogon"
                while pgrep -x "pepLogon" > /dev/null; do
                    sleep 1
                done
                break;;
        [Nn]* ) break;;
        * ) echo "Please answer yes (y) or no (n).";;
    esac
done

# Start shell with autocompletion, preferably with zsh
if type zsh >/dev/null 2>&1; then
  # Start interactive zsh, executing some commands first, see https://stackoverflow.com/a/64793707
  zsh -i --nozle <<< "clear; PROMPT='%n@%m %~ %# '; fpath=(\"$PEP_SCRIPT_DIR/autocomplete_pep.zsh\" \$fpath) && autoload -Uz compinit && compinit && exec </dev/tty && setopt zle && clear"
else
  # bash autocomplete doesnt work currently
  bash --init-file "$PEP_SCRIPT_DIR/autocomplete_pep.bash"
fi