# Load in PowerShell to enable completions
# Compatible with both Windows Powershell <=v5 (powershell) and Powershell (Core) v6+ (pwsh)
# Install by adding to $PROFILE (and restart shell)

# Reference: https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/register-argumentcompleter

using namespace System.Management.Automation

$pep_bins = '@cli_executables@' -split ' '
# cli_executables will be replaced by CMake with all binary names

Register-ArgumentCompleter -Native -CommandName $pep_bins -ScriptBlock {
  param(
    [string] $wordToComplete,
    [Language.CommandAst] $commandAst,
    [int] $cursorPosition
  )

  $exe = $commandAst.GetCommandName()
  if (!$exe) { return }

  &$exe --autocomplete ($commandAst.CommandElements | select -Skip 1 | where { $_.Extent.EndOffset -lt $cursorPosition }) |
  # For each CompletionEntry
  % {
    # _discard is to allow for new fields to be added
    $entryType, $completionType, $completionKey, $valuesStr, $valueType, $_discard = $_ -split [char]0x1c
    # Extensibility: maybe we want to add other entry types later
    if ($entryType -ne 'suggest') { return }
    $(
      # For each CompletionValue
      if ($valuesStr) {
        $valuesStr -split [char]0x1d | % {
          $valueAliasesStr, $displayValue, $description, $_discard = $_ -split [char]0x1e
          $valueAliases = $valueAliasesStr -split [char]0x1f
          if (!$displayValue) {
            $displayValue = $valueAliases -join '/'
          }
          $completionResultType = switch ($completionType) {
            'subcommands' { [CompletionResultType]::Command }
            'parameters' { [CompletionResultType]::ParameterName }
            'values' { [CompletionResultType]::ParameterValue }
            Default { [CompletionResultType]::Text }
          }
          if (!$description) {
            $description = $displayValue
          }
          [CompletionResult]::new($valueAliases[0], $displayValue, $completionResultType, $description)
        }
      }

      switch ($valueType) {
        'file' { Get-ChildItem | % { [CompletionResult]::new($_.Name) } }
        'directory' { Get-ChildItem -Directory | % { [CompletionResult]::new($_.Name) } }
      }
    ) | where { $_.CompletionText.StartsWith($wordToComplete) }
  }
}
