#!/usr/bin/env bash

set -euo pipefail

## Summary:
# The 'pep_download_tool.sh' script is a minimal wrapper around pepcli for downloading all accessible contents for a user and convert the pulled data to csv.


######################################################################################### for downloading all accessible contents for a user and convert the pulled data to csv.


############################ Bash script argument parsing ################################
##########################################################################################
function print_cmd_usage { # cli overview of parameters
  echo "Usage: pep_download_tool.sh [--flatpak-env/-E <env>] [--help/-h] etc...
  
  Switches: 
    --flatpak-env/-E                Pass a Flatpak Environment. E.g. something like nl.ru.cs.pep.ENV-NAME
    --help/-h                       This parameter overview.
    .";
}

# Substitute long options to short ones (thnx https://stackoverflow.com/a/30026641/3906500 )
for arg in "$@"; do
  shift
  case "$arg" in
    '--flatpak-env')      set -- "$@" '-E'   ;;
    '--help')             set -- "$@" '-h'   ;;
    *)                    set -- "$@" "$arg" ;;
  esac
done

PEPCLI_EXECUTABLE="pepcli"
PEPLOGON_EXECUTABLE="pepLogon"

while getopts 'hE:' flag; do
  case "${flag}" in
    E)  # provide Linux Flatpak Environment
            LINUX_FLATPAK_ENV="$OPTARG";
            PEPCLI_EXECUTABLE="flatpak run --command=pepcli $LINUX_FLATPAK_ENV "
            PEPLOGON_EXECUTABLE="flatpak run --command=pepLogon $LINUX_FLATPAK_ENV"
            ;;
    h) # show help message
            print_cmd_usage
            exit 0
            ;;
    ?)
            console_alert "Unknown parameter in $*."
            print_cmd_usage
            exit 2
            ;;
  esac
done # getopts
############################ End of Bash script argument parsing ################################





function pepWrapUnsupported {
  echo "Ahh, thou art using $1, we have heard a lot about your operating system, but do not support it at this time."
  exit 1
}


function zenityCheck {
  set +e
    if command -v zenity >/dev/null 2>&1; then
        echo "Check zenity availability: OK. "
    else
        echo "ERROR: zenity not found. This script requires zenity to work. Please install zenity and try again. 
        (e.g. on Debian based: 'sudo apt-get install zenity') "
        exit 1
    fi
  set -e
}



function pepWrapLinux {

## Check if zenity is installed for user interfacing
zenityCheck

set +e
  output_dir=$(zenity --filename="$(xdg-user-dir DOWNLOAD)" --title="Please select the output directory for your files:" --file-selection --directory 2> /dev/null)
  zenityOutputDirExitCode="$?"
set -e

case "$zenityOutputDirExitCode" in
      0)
        echo "\"$output_dir\" selected."
        ;;
      1)
        echo "No file selected, now exiting pepWrapper."
        exit 1
        ;;
      -1)
        echo "An unexpected error has occurred."
        exit 1
        ;;
esac


pwd
# echo "Moving to output dir: $output_dir"
cd "$output_dir" || exit 
pwd

# Call pepLogon executable
$PEPLOGON_EXECUTABLE

enrolled_msg=$($PEPCLI_EXECUTABLE query enrollment)

version_info=$($PEPCLI_EXECUTABLE --version)

set +e
  zenity --question --title "Start pull all accessible?" --text "$enrolled_msg \n\nVersion info:$version_info \n\nStart pulling all accessible data to $output_dir? \n\n(please be patient after proceeding, pulling data may take a while depending on the size of the dataset)"  2> /dev/null
  zenityStartPullExitCode="$?"
set -e

  case "$zenityStartPullExitCode" in
    0) #Answer yes:
      # Start pulling data. Make sure to use the correct pepcli binary.
      $PEPCLI_EXECUTABLE pull --all-accessible --force --export csv | zenity --progress --no-cancel --title "Pulling data from PEP Repo." --pulsate --auto-close
      # Open file browser?
      xdg-open .
      ;;
    1)
      echo "Operation cancelled."
      exit 1 # no further question your honour
      ;;
    -1)
      echo "An unexpected error has occurred."
      exit 1 # crashing early
      ;;
  esac

}



case "$OSTYPE" in
  solaris*) 
     pepWrapUnsupported "SOLARIS"
     ;;
  darwin*) 
     pepWrapUnsupported "OSX"
     ;; 
  linux*)
     pepWrapLinux
     ;;
  bsd*)     
     pepWrapUnsupported "BSD"
     ;;
  msys*) 
     pepWrapUnsupported "WINDOWS"
     ;;
  *)        echo "unknown operating system family: $OSTYPE" ;;
esac


