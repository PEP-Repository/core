# This file starts with a UTF-8 BOM to make sure the emojis work
param([string]$PepProject, [string]$PepEnvironment)

$host.ui.RawUI.WindowTitle = "PEP $PepProject $PepEnvironment Command Prompt"
$env:Path = "$PSScriptRoot;$env:Path"

# Load our autocompletions
autocomplete_pep.ps1
# Always use detailed completion menu
Set-PSReadLineKeyHandler -Key Tab -Function MenuComplete
# Set encoding for Out-File and redirects to UTF-8 (with BOM) instead of UTF-16 LE,
# see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2276, https://stackoverflow.com/a/40098904
$PSDefaultParameterValues['Out-File:Encoding'] = 'utf8'

Set-Location ~/Downloads

function prompt {
  $prefix = if ($?) {'✔️'} else {'❌'}
  "$prefix`n[$PepProject $PepEnvironment] $($executionContext.SessionState.Path.CurrentLocation)`n$('>' * ($nestedPromptLevel + 1)) ";
}

Write-Output @"
Using PEP binaries at $PSScriptRoot
Tab-completion is available

"@
