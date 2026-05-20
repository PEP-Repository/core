#!/usr/bin/env sh

set -eu

# Initialize variables
SOURCE_PATH=""
DEST_PATH=""
BUILD_MODE="."
EXTENSION=""
FILES=""

# Function to display usage. Specify exit code as a parameter: 0 for success or a different value for failure
usage() {
  echo "Usage: $0 --source-prefix <source_prefix> --dest-prefix <destination_path> --build-mode <build_mode> --append-extension <extension> <files...>"
  exit $1
}

# Function to get size of a path in KB
get_size_kb() {
  du -sk "$1" | cut -f1
}

# Function to convert KB to MB with 2 decimal places
kb_to_mb() {
  echo "$1" | awk '{printf "%.2f", $1 / 1024}'
}

# Parse command-line options
while [ "$#" -gt 0 ]; do
  case $1 in
    --source-prefix) SOURCE_PATH="$2"; shift ;;
    --dest-prefix) DEST_PATH="$2"; shift ;;
    --build-mode) BUILD_MODE="$2"; shift ;;
    --append-extension) EXTENSION="$2"; shift ;;
    --help) usage 0; ;;
    *) FILES="$FILES $1" ;;
  esac
  shift
done

# Check if required options are provided
if [ -z "$SOURCE_PATH" ] || [ -z "$DEST_PATH" ]; then
  usage 1
fi

# Ensure destination path exists
mkdir -p "$DEST_PATH"

TOTAL_SIZE_KB=0

for filepath in $FILES; do
  if [ -d "$SOURCE_PATH/$filepath" ]; then # If path is a directory, copy it recursively
    # Don't process $BUILD_MODE and/or $EXTENSION in this case: we'll find (executable) files
    # regardless of output (sub)directory and file extension.
    cp -R "$SOURCE_PATH/$filepath" "$DEST_PATH/$filepath"
    
    # Get size of the copied directory
    SIZE_KB=$(get_size_kb "$DEST_PATH/$filepath")
    SIZE_MB=$(kb_to_mb "$SIZE_KB")
    echo "Staged $filepath (${SIZE_MB} MB)"
    TOTAL_SIZE_KB=$((TOTAL_SIZE_KB + SIZE_KB))
  else # If path is a file, copy it normally
    FULL_SOURCE_PATH="$SOURCE_PATH/$(dirname "$filepath")/$BUILD_MODE/$(basename "$filepath")"
    # Append extension (e.g. ".exe") if the specified file (name) doesn't exist
    if [ ! -f "$FULL_SOURCE_PATH" ]; then
      FULL_SOURCE_PATH="${FULL_SOURCE_PATH}${EXTENSION}"
    fi

    # Ensure the destination directory exists
    DEST_DIR="$DEST_PATH/$(dirname "$filepath")"
    mkdir -p "$DEST_DIR"
    cp "${FULL_SOURCE_PATH}" "$DEST_DIR"
    
    # Get size of the copied file
    SIZE_KB=$(get_size_kb "$DEST_DIR/$(basename "$filepath")${EXTENSION}")
    SIZE_MB=$(kb_to_mb "$SIZE_KB")
    echo "Staged $filepath (${SIZE_MB} MB)"
    TOTAL_SIZE_KB=$((TOTAL_SIZE_KB + SIZE_KB))
  fi
done

TOTAL_SIZE_MB=$(kb_to_mb "$TOTAL_SIZE_KB")
echo ""
echo "Files staged successfully"
echo "Total size: ${TOTAL_SIZE_MB} MB"
