# This file is meant to be autoloaded
# Install it under `pkg-config --variable=completionsdir fish` (and restart shell)
#  as e.g. /usr/share/fish/vendor_completions.d/pepcli.fish
#  (the name must be equal to the name of the command you want to complete)
# Or source directly for testing

# Reference: https://fishshell.com/docs/current/completions.html

function __fish_complete_pep
  # Command excluding current token
  set partial_cmd (commandline --current-process --tokenize --cut-at-cursor)
  set current_token (commandline --current-token --cut-at-cursor)

  $partial_cmd[1] --autocomplete $partial_cmd[2..] 2>/dev/null |
  # For each CompletionEntry
  while read -l --delimiter \x1c entryType completionType completionKey valuesStr valueType _discard
    [ $entryType != suggest ] && continue

    if [ -n $valuesStr ]
      # For each CompletionValue
      string split \x1d -- $valuesStr |
      while read -l --delimiter \x1e valueAliasesStr displayValue description _discard
        set -l valueAliases (string split \x1f -- $valueAliasesStr)
        # Value to complete with description, separated by \t
        echo $valueAliases[1]\t$description
      end
    end

    switch $valueType
      case file
        __fish_complete_path $current_token $completionKey
      case directory
        __fish_complete_directories $current_token $completionKey
    end
  end
end

# cli_executables will be replaced by CMake with all binary names
for bin in @cli_executables@
  complete --command $bin --no-files --arguments '(__fish_complete_pep)'
end
