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

for filepath in $FILES; do
  echo "Staging $filepath"
  if [ -d "$SOURCE_PATH/$filepath" ]; then
    # If path is a directory, copy it recursively
    # Don't process $BUILD_MODE and/or $EXTENSION in this case: we'll find (executable) files
    # regardless of output (sub)directory and file extension.
    cp -R "$SOURCE_PATH/$filepath" "$DEST_PATH/$filepath"
  else
    FULL_SOURCE_PATH="$SOURCE_PATH/$(dirname "$filepath")/$BUILD_MODE/$(basename "$filepath")"
    DEST_DIR="$DEST_PATH/$(dirname "$filepath")"

    # Ensure the destination directory exists
    mkdir -p "$DEST_DIR"
    # If path is a file, copy it normally
    cp "${FULL_SOURCE_PATH}${EXTENSION}" "$DEST_DIR"
  fi
done

echo "Files staged successfully"
