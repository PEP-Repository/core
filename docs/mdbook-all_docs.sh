#!/usr/bin/env bash

# An example script that uses md2mdbook.sh (from scripts/bash_misc/conversion) to generate an mdbook tree on %CORE%/docs/ in %CORE%/temp/docs_output 

mkdir ../temp 2> /dev/null
mkdir ../temp/docs_output 2> /dev/null
cd ../temp/docs_output || exit
../../scripts/bash_misc/conversion/md2mdbook.sh --mdSourcePath ../../docs/

