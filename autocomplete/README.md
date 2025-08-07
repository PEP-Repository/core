# Autocompletion (tab completion) for PEP command line tools

All PEP C++ CLI Commands have an `--autocomplete` switch that outputs completion suggestions in a shell-agnostic machine-readable format. Different completion categories (e.g. subcommands vs parameter names vs parameter values) are separated by newlines, while fields and values on each line are separated by [ASCII separators](https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Basic_ASCII_control_codes) 0x1C (top) - 0x1F (bottom). In this way, values do not need to be quoted or escaped in some way, assuming they do not contain these separators (if they do, completion results will be messed up, no big deal). The `--autocomplete` switch can be applied to partial commands as well to get suggestions for the word following the end of the partial command, e.g. `pepcli --autocomplete pull --output-directory`. For files and directories, the category ('file'/'directory') is suggested instead of the actual files in the current directory.

The output from the `--autocomplete` command is the parsed by a shell-specific script providing the autocompletions, see `./autocomplete_pep.*`. These scripts need to be installed in some way to activate the completions, see `install_autocomplete_pep*`. Placeholders for binary names inside scripts are replaced by `/CMakeLists.txt`, resulting into usable scripts with the same names inside `/build/autocomplete/`.

## Install

Autocompletion is integrated into the Docker images via `/docker/*.Dockerfile`, but not for Flatpak images.
It can also be installed on the system by running `/build/autocomplete/install_autocomplete_pep*` as root for the right system after running CMake configure. You may need to restart your shell after installation.

Troubleshooting:

- **ZShell**: If you use zsh and autocompletion does not work, you may need to autoload and run `compinit` in `~/.zshrc`. This can be easily done by starting `autoload -Uz compinstall && compinstall` and just saving the defaults.

Autocompletion usually will not work for command aliases (`alias pepcli=...`).
