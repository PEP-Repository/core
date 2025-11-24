$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

echo 'Starting script for Windows CI runner provisioning.'

# Define PEP_NO_PROVISION_RUNNER to avoid installing software
if (!Test-Path env:PEP_NO_PROVISION_RUNNER) {
  echo 'Installing / upgrading packages'

  $pkgs = @(
    'Microsoft.PowerShell' # Powershell Core
    'Git.Git'
    'WiXToolset.WiXToolset' # =v3 (legacy)
    'Microsoft.Sysinternals.PsTools' # PsExec64 etc.
    'JFrog.Conan'
    'GoLang.Go'
  )
  winget install $pkgs --disable-interactivity

  <#
  TODO Install gitlab-runner? It won't do us any good during CI builds (when it must already be present),
  but will make it easy to provision a machine as a runner by interactively running this script. Make it nice and have the script
  - gitlab-runner is configured for the "shell" executor.
  - gitlab-runner's shell is set to "pwsh".
  - gitlab-runner service runs under a non-service administrative user account.
  #>

  echo 'Done installing'

  function InstallLatestVisualStudio {
    # https://gist.github.com/robotdad/83041ccfe1bea895ffa0739192771732
    # https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio
    winget install Microsoft.VisualStudio.Community --disable-interactivity `
      --override '--wait --passive --add ProductLang En-us --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended'
  }

  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (!(Test-Path $vswhere)) {
    echo 'Visual Studio not found'
    InstallLatestVisualStudio
  }

  # Reload some environment variables for just-installed programs.
  # See also https://github.com/microsoft/winget-cli/issues/3077
  $env:Path = [Environment]::GetEnvironmentVariable('Path', [EnvironmentVariableTarget]::Machine) +
    ';' + [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::User)
  $env:WIX = [Environment]::GetEnvironmentVariable('WIX', [EnvironmentVariableTarget]::Machine)

  git config --global core.autocrlf true

  echo 'Installing Go plugin for Protocol Buffers'
  go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
} else {
  echo 'PEP_NO_PROVISION_RUNNER is set; will not install software'
}

$vswhere_extra_args = @()

# Set PEP_USE_EXISTING_VS_VERSION_RANGE to e.g. '[17, 18)' to force using already installed version 2022
# See https://aka.ms/vswhere/versions
if (Test-Path env:PEP_USE_EXISTING_VS_VERSION_RANGE) {
  echo "PEP_USE_EXISTING_VS_VERSION_RANGE: $env:PEP_USE_EXISTING_VS_VERSION_RANGE"
  $vswhere_extra_args += @('-version'; $env:PEP_USE_EXISTING_VS_VERSION_RANGE)
} elseif (!Test-Path env:PEP_NO_PROVISION_RUNNER) {
  echo 'Checking for Visual Studio updates'
  InstallLatestVisualStudio
}

# Export environment variables for other scripts

# E.g. 'C:\Program Files\Microsoft Visual Studio\18\Community'
$env:PEP_VISUAL_STUDIO_DIR = &$vswhere $vswhere_extra_args -latest -property resolvedInstallationPath
echo "Using Visual Studio installation at $env:PEP_VISUAL_STUDIO_DIR"

$env:PEP_VCVARS_BAT = "$env:PEP_VISUAL_STUDIO_DIR\VC\Auxiliary\Build\vcvars64.bat"

echo 'Finished script for Windows CI runner provisioning.'
