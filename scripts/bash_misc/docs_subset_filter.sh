#!/usr/bin/env bash

## Summary:
## This script was built to maintain the 'single source of truth' concept in documentation, filtering minimalised versions of e.g. diagram from more elaborate source versions.

# Some colorful output macro's to separate status msg from the rest.
function console_update { LINEEND="${2:-\n}"; MSG_COLOR="${3:-0;102m}"; printf "\033[%s%s\033[0m%b" "$MSG_COLOR" "$1" "$LINEEND"; }
function console_alert { console_update "$1" "$2" "0;41m"; }

# Predefining some 'constants' (quoted since they may be overwritten by arguments)
PARSING_CODE_START="erDiagram"
PARSING_CODE_STOP="\`\`\`"
FIRSTLINE_PREFIX="#" # The first lines tells the viewer that it is viewing a subset-export (not to be edited). Display instruction via this prefix.
FILTER_POSITION="AFTER" # Options: "AT" "AFTER" and "ENCLOSED"

OUTPUT_FILE=""

##########################################################################################
############################ Bash script argument parsing ################################
##########################################################################################
function print_cmd_usage { # cli overview of parameters
  console_update "Usage: docs_subset_filter.sh <switches> source_filename filter_string
  (switches must preceed positional arguments)
  
  Switches: 
    --filter-position/-F            Filter Position (only filter lines *at* or *after* the line with the tag). Available modes: 'at', 'after' (default), 'enclosed' (enclosing tags require separate lines)
    --filter-close-tag/-C           Close tag when using 'enclosed' filter position.
    --output-filename/-O            Filename for subset output file. By default the input-filename will be used with a '-export' insert right before the extension.
    --parsing-code-start/-P         Start parsing based on tags only after this start code (and copy everything before), default value: erDiagram
    --parsing-code-stop/-S          Start parsing based on tags only after this start code (and copy everything before), default value: \`\`\`
    --firstline-prefix/-f           The subset output file will start with a line stating that it is derived from the source file. This line is usually a comment or a header, that is preceeded by this string (default: #).
    --help/-h                       This parameter overview.
    .";
}
# Substitute long options to short ones (thnx https://stackoverflow.com/a/30026641/3906500 )
for arg in "$@"; do
  shift
  case "$arg" in
    '--filter-position')    set -- "$@" '-F'   ;;
    '--filter-close-tag')   set -- "$@" '-C'   ;;
    '--output-filename')    set -- "$@" '-O'   ;;
    '--parsing-code-start') set -- "$@" '-P'   ;;
    '--parsing-code-stop')  set -- "$@" '-S'   ;;
    '--firstline-prefix')   set -- "$@" '-f'   ;;
    '--help')               set -- "$@" '-h'   ;;
    *)                      set -- "$@" "$arg" ;;
  esac
done
while getopts 'hF:O:C:P:S:f:' flag; do
  case "${flag}" in
    h)      # show help message
            print_cmd_usage
            exit 2
            ;;

    F)      # Filter Position (only filter lines *at* or *after* the line with the tag). Available modes: 'at' and 'after'
            if [ -n "$OPTARG" ]; then
                if [[ "$OPTARG" == "after" ]]; then
                    FILTER_POSITION="AFTER"
                elif [[ "$OPTARG" == "at" ]]; then
                    FILTER_POSITION="AT"
                elif [[ "$OPTARG" == "enclosed" ]]; then
                    FILTER_POSITION="ENCLOSED"
                else
                    console_alert "ERROR: filter position should be either \"after\", \"at\" or \"enclosed\" "
                    exit 2
                fi
            fi
            ;;
    
    O)      OUTPUT_FILE="$OPTARG"
            ;;
    C)      FILTER_CLOSE_TAG="$OPTARG"
            ;;
    P)      PARSING_CODE_START="$OPTARG"
            ;;
    f)      FIRSTLINE_PREFIX="$OPTARG"
            ;;
    S)      PARSING_CODE_STOP="$OPTARG"
            ;;
    ?)
            console_alert "Unknown parameter in $*."
            print_cmd_usage
            exit 2
            ;;
  esac
done # getopts
shift $((OPTIND-1))
############################ End of Bash script argument parsing ################################


# Check if there are at least 3 parameters
if [ "$#" -lt 2 ]; then
    console_alert "Error: At least two parameters are required."
    console_update "Syntax: 1) source filename 2) %%TAG to filter"
    exit 1
fi

# Check if the first parameter is a filename that exists
if [ ! -f "$1" ]; then
    console_alert "Error: The first parameter must be a valid filename that exists."
    exit 1
fi

# Input and output files
INPUT_FILE="$1"
FILTER_TAG="$2"

if [[ -z "$OUTPUT_FILE" ]]; then
    console_update "No output file given ($OUTPUT_FILE), generating instead based on $INPUT_FILE"
    OUTPUT_FILE="${INPUT_FILE%%.*}-output.${INPUT_FILE#*.}"
fi

# Check if the output file already exists
if [ -f "$OUTPUT_FILE" ]; then
    # Prompt the user for overwrite confirmation
    read -p "File '$OUTPUT_FILE' exists. Do you want to overwrite it? (y/n): " answer

    # Convert the answer to lowercase for consistency
    answer=$(echo "$answer" | tr '[:upper:]' '[:lower:]')

    # Check the user's answer
    if [[ "$answer" == "y" || "$answer" == "yes" ]]; then
        console_update "Overwriting the file..."
        # Continue with the rest of the script (add your commands here)
    else
        console_alert "Exiting without overwriting."
        exit 0
    fi
fi

console_update "Filtering \"$INPUT_FILE\" on \"$FILTER_TAG\" > \"$OUTPUT_FILE\""


# Clear the output file
echo "$FIRSTLINE_PREFIX Generated by filtering $INPUT_FILE on $FILTER_TAG"  > "$OUTPUT_FILE"

# Function used to write/copy lines to output file in code below.
add_line ( ) {
    echo ":> $1"
    echo "$1" >> "$2"
}

# Filter flag status:
filter_flag_status=false
# Flag to determine if we are within the 'erDiagram' and '```' region
in_erdiagram=false

# Read the input file line by line
while IFS= read -r line; do
    # Check for the start of the 'erDiagram' section
    if [[ "$line" == *"$PARSING_CODE_START"* ]]; then
        in_erdiagram=true
    add_line "$line" "$OUTPUT_FILE"
        continue
    fi

    # Check for the end of the 'erDiagram' section
    #if [[ "$line" == *'```'* ]] && $in_erdiagram; then
    if [[ "$line" == *"$PARSING_CODE_STOP"* ]] && $in_erdiagram; then
        in_erdiagram=false
    add_line "$line" "$OUTPUT_FILE"
        continue
    fi

    # If within the 'erDiagram' section, only copy lines after a line with '$FILTER_TAG'
    if $in_erdiagram; then

        # If previous line iteration flagged the next line for filtering:
        if [[ "$FILTER_POSITION" == "AFTER" && "$filter_flag_status" == "true" ]]; then
            add_line "$line" "$OUTPUT_FILE"
            filter_flag_status=false
        fi

        # shorthand if ... then ...
        [[ "$line" == *"$FILTER_TAG"* ]] && filter_flag_status="true"
        [[ "$FILTER_POSITION" == "ENCLOSED" && "$line" == *"$FILTER_CLOSE_TAG"* ]] && filter_flag_status="false"

        # Current line iteration set true
        if [[ "$FILTER_POSITION" == "AT" && "$filter_flag_status" == "true" ]]; then
                add_line "$line" "$OUTPUT_FILE"
                filter_flag_status=false
        fi

        [[ "$FILTER_POSITION" == "ENCLOSED" && "$filter_flag_status" == "true" ]] && add_line "$line" "$OUTPUT_FILE"

    else
    # If not in 'erDiagram' section, copy the line as is
    add_line "$line" "$OUTPUT_FILE"
    fi
done < "$INPUT_FILE"

console_update "Filtered copy saved to $OUTPUT_FILE."

