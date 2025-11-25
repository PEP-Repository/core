#Requires -PSEdition Core
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true # PowerShell Core only

if (!(Test-Path env:PEP_NO_SOFTWARE_INSTALL)) {
  echo 'Upgrading Conan'
  pipx upgrade conan
}

$vswhere_extra_args = @()

# Set PEP_USE_VS_VERSION_RANGE to force using specific version. E.g. '[17, 18)' to use version 2022 (which must be installed).
# See https://aka.ms/vswhere/versions
if (Test-Path env:PEP_USE_VS_VERSION_RANGE) {
  echo "PEP_USE_VS_VERSION_RANGE: $env:PEP_USE_VS_VERSION_RANGE"
  $vswhere_extra_args += @('-version'; $env:PEP_USE_VS_VERSION_RANGE)
}

# Export environment variables for other scripts

# E.g. 'C:\Program Files\Microsoft Visual Studio\18\Community'
$env:PEP_VISUAL_STUDIO_DIR = &$vswhere $vswhere_extra_args -latest -property resolvedInstallationPath
if (!$env:PEP_VISUAL_STUDIO_DIR) {
  throw "Visual Studio $vswhere_extra_args not installed"
}
echo "Using Visual Studio installation at $env:PEP_VISUAL_STUDIO_DIR"

$env:PEP_VCVARS_BAT = "$env:PEP_VISUAL_STUDIO_DIR\VC\Auxiliary\Build\vcvars64.bat"
