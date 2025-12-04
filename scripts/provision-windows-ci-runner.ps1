<#
.SYNOPSIS
  Provision/update Windows Runner. Run as gitlabrunner on the machine.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

if (!($env:USERNAME -match 'gitlab')) {
  throw 'This script must be run as gitlabrunner user'
}

echo 'Installing / upgrading packages'

# System-wide installs (some of these could've been per-user)
$pkgs = @(
  'Microsoft.PowerShell' # Powershell Core
  'Git.Git'
  'GoLang.Go'
)
winget install $pkgs --accept-package-agreements --disable-interactivity
# 0x8A15002B = no upgrade found: see https://github.com/microsoft/winget-cli/blob/master/doc/windows/package-manager/winget/returnCodes.md
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 0x8A15002B) { exit $LASTEXITCODE }

winget install WiXToolset.WiXToolset --version=3.14.1.8722 --accept-package-agreements --disable-interactivity
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 0x8A15002B) { exit $LASTEXITCODE }

# Per-user install
echo 'Installing Python'
$pkgs = @(
  'Python 3.13'
)
winget install $pkgs --source msstore --accept-package-agreements --disable-interactivity
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 0x8A15002B) { exit $LASTEXITCODE }

# System-wide install
echo 'Installing Visual Studio'
# https://gist.github.com/robotdad/83041ccfe1bea895ffa0739192771732
# https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio
winget install Microsoft.VisualStudio.Community --accept-package-agreements --disable-interactivity `
	--override '--wait --passive --add ProductLang En-us --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended'
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 0x8A15002B) { exit $LASTEXITCODE }

<#
TODO Install gitlab-runner. Make it nice and have the script
- gitlab-runner is configured for the "shell" executor.
- gitlab-runner's shell is set to "pwsh".
- gitlab-runner service runs under a non-service non-administrator user account.
#>

function ReloadPath {
  # Reload Path for just-installed programs.
  # See also https://github.com/microsoft/winget-cli/issues/3077
  $env:Path = [Environment]::GetEnvironmentVariable('Path', [EnvironmentVariableTarget]::Machine) +
    ';' + [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::User)
}

ReloadPath

if (!(Get-Command python3)) {
  throw 'python3 not found. Go to Manage App Execution Aliases and enable the right one'
}

echo 'Installing pipx'
python3 -m pip install pipx --user --upgrade --upgrade-strategy eager
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

python3 -m pipx ensurepath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ReloadPath

echo 'Installing Conan'
pipx install 'conan==2.*'
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

pipx upgrade-all
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

echo 'Done installing'

git config --global core.autocrlf true
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

echo 'Installing Go plugin for Protocol Buffers'
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
