@echo off
REM Variables are prefixed with "PEP" to reduce chances of naming collisions.

echo Starting script for Windows CI runner provisioning.

set PEP_CHOCO_DIR=%ALLUSERSPROFILE%\chocolatey

set PEP_CHOCO_EXE="%PEP_CHOCO_DIR%\bin\choco.exe"
if not exist %PEP_CHOCO_EXE% (
  echo Installing Chocolatey.
  "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" || exit /B 1
  if not exist %PEP_CHOCO_EXE% (
    echo Chocolatey was not installed to the expected location. Aborting.
    exit /B 1
  ) else (
    %PEP_CHOCO_EXE% feature enable -n allowGlobalConfirmation || exit /B 1
  )
)

REM Installation of Git won't do us any good during CI builds (when it must already be present),
REM but helps when this script is run interactively.
SET PEP_GIT_DIR=C:\Program Files\Git
SET PEP_GIT_EXE="%PEP_GIT_DIR%\cmd\git.exe"
if not exist "%PEP_GIT_DIR%" (
  echo Installing Git.
  %PEP_CHOCO_EXE% install git -y -params '/COMPONENTS="assoc_sh"' || exit /B 1
  if not exist %PEP_GIT_EXE% (
    echo Git was not installed to the expected location. Aborting.
    exit /B 1
  ) else (
    echo Configuring CRLF handling for Git.
    %PEP_GIT_EXE% config --global core.autocrlf true || exit /B 1
  )
)

REM TODO Install gitlab-runner. It won't do us any good during CI builds (when it must already be present),
REM but will make it easy to provision a machine as a runner by interactively running this script. Make it nice and have the script
REM - gitlab-runner is configured for the "shell" executor.
REM - gitlab-runner's shell is set to "powershell".
REM - gitlab-runner service runs under a non-service administrative user account.

SET PEP_VISUAL_STUDIO_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community
if not exist "%PEP_VISUAL_STUDIO_DIR%" (
  echo Installing Visual Studio 2022 Community.
  %PEP_CHOCO_EXE% install visualstudio2022community -y || exit /B 1
  if not exist "%PEP_VISUAL_STUDIO_DIR%" (
    echo Visual Studio was not installed to the expected location. Aborting.
    exit /B 1
  )
)

SET PEP_VCVARS_BAT="%PEP_VISUAL_STUDIO_DIR%\VC\Auxiliary\Build\vcvars64.bat"
if not exist %PEP_VCVARS_BAT% (
  echo Installing Visual Studio 2022 native desktop workload.
  REM The --package-parameters could instead "--add Some.Package.Id;Another.Package.Id", reducing installation footprint.
  REM For a list of package IDs for Visual Studio 2022, see
  REM https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-community?view=vs-2022&preserve-view=true
  %PEP_CHOCO_EXE% install visualstudio2022-workload-nativedesktop -y --package-parameters "--includeRecommended --includeOptional" || exit /B 1
  if not exist %PEP_VCVARS_BAT% (
    echo Visual Studio native desktop workload was not installed to the expected location. Aborting.
    exit /B 1
  )
)

REM TODO Support installing newer versions, see https://gitlab.pep.cs.ru.nl/pep/core/-/commits/fix-wix-update
REM Store command output into a variable: https://stackoverflow.com/a/2340018
for /f "tokens=*" %%i in ('dir /b "%ProgramFiles(x86)%\WiX Toolset v3.*"') do set PEP_WIX_DIR=%ProgramFiles(x86)%\%%i
if not exist "%PEP_WIX_DIR%" (
  echo Installing WiX toolset.
  %PEP_CHOCO_EXE% install wixtoolset --version 3.11.2 -y || exit /B 1
  if not exist "%PEP_WIX_DIR%" (
    echo WiX toolset was not installed to the expected location. Aborting.
    exit /B 1
  )
)
for /f "tokens=*" %%i in ('dir /b "%ProgramFiles(x86)%\WiX Toolset v3.*"') do set PEP_WIX_DIR=%ProgramFiles(x86)%\%%i
REM If WiX was just installed the "WIX" environment variable won't be available
REM in the current session yet. Ensure the variable is provided to other scripts.
REM We do so outside the "if" statement above because the closing parenthesis
REM in %PEP_WIX_DIR% interferes with scoping.
if "%WIX%" == "" set WIX=%PEP_WIX_DIR%

set PEP_PSEXEC64_EXE="%PEP_CHOCO_DIR%\lib\pstools\tools\PsExec64.exe"
if not exist %PEP_PSEXEC64_EXE% (
  echo Installing SysInternals PsTools.
  %PEP_CHOCO_EXE% install pstools -y || exit /B 1
  if not exist %PEP_PSEXEC64_EXE% (
    echo PsTools were not installed to the expected location. Aborting.
    exit /B 1
  )
)

echo Installing / updating Conan.
%PEP_CHOCO_EXE% upgrade conan --install-if-not-installed -y || exit /B 1

echo Installing / updating Go language.
%PEP_CHOCO_EXE% upgrade golang --install-if-not-installed -y || exit /B 1

refreshenv

echo Installing Go plugin for Protocol Buffers.
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest || exit /B 1

echo Finished script for Windows CI runner provisioning.
