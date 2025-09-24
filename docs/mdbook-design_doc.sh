#!/usr/bin/env bash

set -eu

# $CORE_ROOT is the core repository root
CORE_ROOT=$(cd "${0%/*}/../" && pwd)

mkdir -p "$CORE_ROOT/temp/design_doc-output/"
cd "$CORE_ROOT/temp/design_doc-output/"

MD2MDBOOKPATH="$CORE_ROOT/scripts/bash_misc/conversion/md2mdbook.sh"

"$MD2MDBOOKPATH" --genPdf --theme "$CORE_ROOT/docs/design_document/.theme" --title "PEP Design Document" --mdSourcePath "../../docs/design_document"
