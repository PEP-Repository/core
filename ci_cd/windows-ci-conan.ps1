#Requires -PSEdition Core

<#
.SYNOPSIS
  Upgrade conan and call conan with provided arguments, synchronizing with other calls of this script.
  Clean cache afterwards if CLEAN_CONAN is set.
#>

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true # PowerShell Core only

# Replace `-[...]__[...]` by `-[...]:[...]`. Workaround for https://github.com/PowerShell/PowerShell/issues/16432.
# (?<=...) = RegEx lookbehind
$ConanArgs = $args -replace '(?<=^-[^_]*)__', ':'

# Set PEP_USE_MSVC_VERSION to force using specific version. E.g. 194 to use some VS 2022 version (which must be installed).
if (Test-Path env:PEP_USE_MSVC_VERSION) {
  $ConanArgs += @('-s:a'; "compiler.version=$env:PEP_USE_MSVC_VERSION")
}

do {
  try {
    $conanLock = [IO.File]::Open("$HOME\.pep-conan-lock", [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::Read)
  }
  catch [IO.IOException] {
    Write-Warning "Waiting for other CI job to finish with Conan: $_"
    Start-Sleep 10s
    continue
  }
} while ($false)

try {
  # Set PEP_NO_SOFTWARE_INSTALL to prevent installing software
  if (!(Test-Path env:PEP_NO_SOFTWARE_INSTALL)) {
    echo 'Upgrading Conan'
    pipx upgrade conan
  }

  conan @ConanArgs

  if (Test-Path env:CLEAN_CONAN) {
    echo 'Cleaning Conan cache.'
    # Remove old packages
    conan remove '*' --lru 4w --confirm
    # Remove some temporary build files (excludes binaries)
    conan cache clean --build --temp
  }
}
finally {
  $conanLock.Dispose()
}
