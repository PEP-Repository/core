#!/usr/bin/env bash

function die() {
    exit 1
}

function invalid_invocation() {
    >&2 echo "Usage: $0 <pipeline> <job> <dir>"
    >&2 echo "  e.g. $0 6543 98765 <build_dir>/wix/stable"
    die
}

if [ "$#" -ne 3 ]; then
    >&2 echo "Incorrect number of command line parameters."
    invalid_invocation
fi

echo "Generating installer metadata XML for directory $3"

# Glob current files into an array to prevent metafile itself from being processed
shopt -s nullglob # Ensure array is empty if there are no files
shopt -s globstar # Allow recursive glob using **
cd "$3" || exit # Ensure paths are relative from the directory root
payloadfiles=(**/*)

metafile=installer.xml

{
    echo "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
    echo "<installer>"
    echo "  <pipeline>$1</pipeline>"
    echo "  <job>$2</job>"
    echo "  <files>"
} > $metafile

for i in "${payloadfiles[@]}"
do
    hash=$(sha512sum "$i" | cut -d " " -f 1 | tr -d '[:space:]')
    {
        echo "    <file>"
        echo "      <path>$i</path>"
        echo "      <hash algorithm=\"sha512\">$hash</hash>"
        echo "    </file>"
    } >> $metafile
done

{
    echo "  </files>"
    echo "</installer>"
} >> $metafile
