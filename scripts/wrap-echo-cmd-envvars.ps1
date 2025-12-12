<#
.SYNOPSIS
  Invokes PowerShell script, then forwards specified environment variables set by this script to CMD by printing SET commands to stdout.
.EXAMPLE
  REM Invoke PowerShell script, then pick up / copy any environment variables that it set (especially PEP_VCVARS_BAT)
  for /f "tokens=*" %%i in ('pwsh -ExecutionPolicy Bypass -File "%~dp0\..\scripts\wrap-echo-cmd-envvars.ps1" ^
    -ScriptPath "%~dp0\windows-ci-find-msvc.ps1" ^
    PEP_VISUAL_STUDIO_DIR PEP_VCVARS_BAT') do (

    echo Setting envvar: %%i
    %%i
  )
#>
[CmdletBinding()]
param(
  # PowerShell script
  [Parameter(Mandatory=$true)]
  [string] $ScriptPath,

  [string[]] $ScriptArgs,

  [Parameter(Mandatory=$true, Position=0, ValueFromRemainingArguments=$true)]
  [string[]] $EnvVars
)

$ErrorActionPreference = 'Stop'

if (!$ScriptArgs) { $ScriptArgs = @() }


# Execute script, avoiding printing to stdout
&$ScriptPath @ScriptArgs | % { [Console]::Error.WriteLine($_) } # Just 1>&2

# Echo each specified environment variable such that CMD can evaluate it
foreach ($envVar in $EnvVars) {
  echo "set $envVar=$((Get-Item env:$envVar).Value)"
}
