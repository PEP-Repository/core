#Requires -PSEdition Core

<#
.SYNOPSIS
  Upgrade conan and call conan with provided arguments, synchronizing with other calls of this script.
  Clean cache afterwards.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true, Position = 0, ValueFromRemainingArguments = $true)]
  [string[]] $ConanArgs
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true # PowerShell Core only

if (!$ConanArgs) { $ConanArgs = @() }

do {
  try {
    $conanLock = [IO.File]::Open("C:\Users\pep\.pep-conan-lock", [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::Read)
  }
  catch [IO.IOException] {
    Write-Warning "Waiting for other CI job to finish with Conan: $_"
    Start-Sleep 10s
    continue
  }
} while ($false)

try {
  if (!(Test-Path env:PEP_NO_SOFTWARE_INSTALL)) {
    echo 'Upgrading Conan'
    pipx upgrade conan
  }

  conan $ConanArgs

  if (Test-Path env:CLEAN_CONAN) {
    echo 'Cleaning Conan cache.'
    # Remove some temporary build files (excludes binaries)
    conan remove "*" --lru 4w --confirm
    # Remove old packages
    conan cache clean --build --temp
  }
}
finally {
  $conanLock.Dispose()
}
