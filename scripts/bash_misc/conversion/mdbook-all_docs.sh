#!/usr/bin/env bash

set -eu

# An example script that uses md2mdbook.sh (from scripts/bash_misc/conversion) to generate an mdbook tree on %CORE%/docs/ in %CORE%/temp/docs_output 


mkdir -p "$(dirname -- "$0")/../../../temp/docs_output/"
cd "$(dirname -- "$0")/../../../temp/docs_output/"
../../scripts/bash_misc/conversion/md2mdbook.sh --mdSourcePath ../../docs/
