# md2mdbook.sh: conversion from md filetree to mdbook output

`/scripts/bash/conversion/md2mdbook.sh` is a wrapper around mdbook, used to convert a directory tree containing .md files to .html and .pdf in the ./book/ dir. It will copy .md files in the current dir to a ./src/ dir if a ./src/ dir is not already present. If ./src/ dir already exists, it will build upon its contents.

## HOWTO:
Put the directory tree with md files in the MDSRCPATH (by default ./mdsrc/) and `md2mdbook.sh` will copy them into a new directory called './src'. The './src' dir is used by mdbook and at this point in time cannot be changed. `md2mdbook.sh` will generate a 'SUMMARY.md' in the './src' dir (if not yet present) that will instruct mdbook to create an index for all the .md files in the tree.

### Note
NOTE: Delete the ./src directory before calling md2mdbook.sh if you want a new SUMMARY.md to be generated (which is usually the case).

When using md2mdbook in an automated environment, use the -W (die on warning) flag that will prevent prompting to continue execution of md2mdbook on warnings.

### Help
Use `./md2mdbook.sh -h` for details on all parameters.

### Example
Example usage of md2mdbook.sh in a dir where to write the mdbook (temporary, configuration and exported) files:
`%PEP_HOME%/core/scripts/bash_misc/conversion/md2mdbook.sh -s ~/dev/pep/core/docs/ `

## TODO:
### Sub-books:
Currently, md2mdbook.sh stops 'parsing' a directory tree when a SUMMARY.md file is found outside the `mdsrc root`. The idea behind this is that very complex 'books' might require different 'subbooks' not to be fully displayed at the top book, and instead reference there.

Yet unfortunately, currently, these subbooks aren't parsed and linked by `mdbook` as desired.