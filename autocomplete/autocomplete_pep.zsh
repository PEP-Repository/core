#compdef @cli_executables@
# cli_executables will be replaced by CMake with all binary names

# This file is meant to be autoloaded
# Install it under /usr/share/zsh/vendor-completions/ (and restart shell)
# Or place it in a directory that you add to your $fpath in ~/.zsrc

# Reference:
# - https://zsh.sourceforge.io/Doc/Release/Completion-Widgets.html#Completion-Widgets
# - https://zsh.sourceforge.io/Doc/Release/Completion-System.html#Completion-System (utility functions like _describe)

# Quickly test completions by removing the _pep statement at the end and executing: source ./autocomplete_pep.zsh; compdef _pep pepcli

_pep() {
  local completion_success=0
  local completed=0
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

    local tag="$completionType"
    # See https://zsh.sourceforge.io/Doc/Release/Completion-System.html#Standard-Tags
    case $tag in
      subcommands)
        tag=commands ;;
      parameters)
        tag=parameters ;;
      values)
        tag=values ;;
    esac

    if [ -n "$valuesStr" ]; then
      # Display values and descriptions, separated by ':'
      declare -a displayValues=()
      # Actual values to complete, same length as $displayValues
      declare -a completeValues=()

      # For each CompletionValue
      while IFS=$'\x1e' read -r -d $'\x1d' valueAliasesStr displayValue description _discard; do
        # Split (s), with (p) for char escape
        # Note that valueAliases will be () if $valueAliasesStr is ""
        declare -a valueAliases=(${(ps(\x1f))valueAliasesStr})

        displayValue="${displayValue:-${(j(/))valueAliasesStr}}"
        displayValues+=("${displayValue//:/\\:}:${description//:/\\:}") # Concatenate with ':', escaping it in the 2 values
        completeValues+=("${valueAliases[1]}")
      done < <(echo -n "$valuesStr"$'\x1d')

      # Note: escaping values for suggestions is handled automatically, except that it doesn't seem to handle spaces well
      _describe -t "$tag" "$completionKey" displayValues completeValues -o nosort
      completion_success=$(($completion_success || !$?))
    else
      _message -e "$tag" "$completionKey"
    fi

    case $valueType in
      file)
        _files
        completion_success=$(($completion_success || !$?))
        ;;
      directory)
        _path_files -/
        completion_success=$(($completion_success || !$?))
        ;;
    esac
  done < <($words[1] --autocomplete $words[2,$CURRENT-1] 2>/dev/null)
  # ^ Call our executable, strip executable and current word from arguments

  if ((!$completed)); then
    _default  # Fallback in case our --autocomplete command failed
  fi

  # Return 1 when no exact match was found to enable corrections
  return $((!$completion_success))
}
_pep
