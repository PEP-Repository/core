<#
.SYNOPSIS
  Facilitate forwarding environment variables from PowerShell script to CMD by printing SET commands to stdout.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)]
  [string] $ScriptPath,

  [string[]] $ScriptArgs,

  [Parameter(Mandatory=$true, Position=0, ValueFromRemainingArguments=$true)]
  [string[]] $EnvVars
)

$ErrorActionPreference = 'Stop'

if (!$ScriptArgs) { $ScriptArgs = @() }


# Execute script, avoiding printing to stdout
&$ScriptPath $ScriptArgs | % { [Console]::Error.WriteLine($_) } # Just 1>&2

# Echo each specified environment variable such that CMD can evaluate it
foreach ($envVar in $EnvVars) {
  echo "set $envVar=$((Get-Item env:$envVar).Value)"
}
