# This file is meant to be sourced from Bash
# Install bash-completion and install this file as e.g. /usr/share/bash-completion/completions/pepcli (the name must be equal to the name of the command you want to complete)
#  and make sure /usr/share/bash-completion/bash_completion is sourced in ~/.bashrc (and restart shell) or in /etc/bash.bashrc (and re-log)
# Or source directly from ~/.bashrc

# Reference: https://www.gnu.org/software/bash/manual/bash.html#Programmable-Completion

declare -a _pep_bins=(@cli_executables@)
# cli_executables will be replaced by CMake with all binary names

# Also prefix all with ./
_pep_bins+=("${_pep_bins[@]/#/./}")

_pep() {
  # Initialize:
  # - cur: current word
  # - prev: previous word
  # - words: array of words including executable and possibly an empty word at the end
  # - cword: index of current word
  # See /usr/share/bash-completion/bash_completion
  local cur prev words cword
  _init_completion || return

  local completed=0

  # Instead of parsing the arguments here, we will call our executable with --autocomplete for suggestions
  # See pep::commandline::Autocomplete (CommandLineCommand.cpp) for the format

  declare -a suggestions
  # read reads until its -d switch (or default \n), and splits read words by $IFS
  # _discard is to allow for new fields to be added
  # The last line must also end with \n
  # For each CompletionEntry
  while IFS=$'\x1c' read -r entryType completionType completionKey valuesStr valueType _discard; do
    # Extensibility: maybe we want to add other entry types later
    if [ "$entryType" != "suggest" ]; then
      continue
    fi
    completed=1

    if [ -n "$valuesStr" ]; then
      # For each CompletionValue
      while IFS=$'\x1e' read -r -d $'\x1d' valueAliasesStr displayValue description _discard; do
        # Use echo -n instead of <<< to prevent trailing \n
        # Note that valueAliases will be () if $valueAliasesStr is ""
        readarray -t -d $'\x1f' valueAliases < <(echo -n "$valueAliasesStr")
        # Do not add other aliases for now
        # Escape special characters because they need to be completed in escaped form
        suggestions+=("$(printf '%q' "${valueAliases[0]}")")
      done < <(echo -n "$valuesStr"$'\x1d')
    fi

    case $valueType in
      file)
        _filedir  # See /usr/share/bash-completion/bash_completion
        ;;
      directory)
        _filedir -d
        ;;
    esac
  done < <("${words[0]}" --autocomplete "${words[@]:1:cword-1}" 2>/dev/null)
  # ^ Call our executable, strip executable and current word from arguments

  if ((!completed)); then
    return
  fi

  local IFS=$'\x1f'
  declare -a words_arg
  if ((${#suggestions})); then
    # Join using $IFS, quote again for compgen command
    words_arg=(-W "${suggestions[*]@Q}")
  fi
  # compgen filters matches using the prefix in $cur
  # compgen splits words in -W by $IFS
  readarray -t more_replies < <(compgen "${words_arg[@]}" -- "$cur")
  COMPREPLY+=("${more_replies[@]}")
} &&
# Register our completion function
# Do not sort completions, fall back to default completion if it fails
complete -o nosort -o default -F _pep "${_pep_bins[@]}"
