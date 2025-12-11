@echo off
REM TODO: rewrite in PowerShell
REM Having this code in a batch file (as opposed to in .gitlab-ci.yml) allows it to be run and debugged locally.
REM Variables are prefixed with "PEP" to reduce chances of naming collisions.
set OwnDir=%~dp0

echo Starting CI job script for Windows build
REM Colored output for e.g. Conan
set CLICOLOR_FORCE=1
SET WhatToBuild=%1
SET BuildType=%2

if [%WhatToBuild%] == [wixlib] (
  echo Building limited targets and WIX library
  set PEP_CONAN_BUILD_ADDITIONALS=False
  set PEP_CMAKE_BUILD_TARGETS=--target pepAssessor pepLogon pepcli
) else if [%WhatToBuild%] == [all-targets] (
  echo Building all targets
  set PEP_CONAN_BUILD_ADDITIONALS=True
  set PEP_CMAKE_DEFINES=-DBUILD_GO_SERVERS=ON -DENABLE_OAUTH_TEST_USERS=OFF -DVERIFY_HEADERS_STANDALONE=ON
) else (
  echo Missing parameter: specify what to build.
  goto :Invocation
)

if [%BuildType%] == [] (
  SET BuildType=Release
)
if [%BuildType%] == [Debug] (
  set PEP_CONAN_PRESET_FLAVOR=debug
) else if [%BuildType%] == [Release] (
  set PEP_CONAN_PRESET_FLAVOR=release
) else (
  echo Unsupported build type: %BuildType%
  goto :Invocation
)

if "%CI_COMMIT_REF_NAME%" == "" (
  echo No CI_COMMIT_REF_NAME specified: performing 'local' build.
  set CI_COMMIT_REF_NAME=local
)
@echo Creating Windows CI artifacts for %CI_COMMIT_REF_NAME% build.

REM Set BUILD_DIR to BUILD_DIRECTORY if defined, otherwise default to "build"
if "%BUILD_DIRECTORY%" == "" (
  set BUILD_DIR=build
) else (
  set BUILD_DIR=%BUILD_DIRECTORY%
)

if exist "%BUILD_DIR%" (
  if "%CI_COMMIT_REF_NAME%" neq "local" (
    echo Build directory already exists.
    exit /B 1
  ) else (
    echo Performing incremental build on existing build directory.
  )
)

echo Installing Conan packages.

REM `__` will be replaced by `:` in script. Workaround for https://github.com/PowerShell/PowerShell/issues/16432.
pwsh -ExecutionPolicy Bypass -File "%OwnDir%\windows-ci-conan.ps1" ^
  install .\docker-build\builder\conan\conanfile.py ^
  --lockfile=.\docker-build\builder\conan\conan-ci.lock ^
  --profile__all=.\docker-build\builder\conan\conan_profile ^
  --build=missing ^
  -s__a build_type="%BuildType%" ^
  -o "&:with_assessor=True" ^
  -o "&:with_servers=%PEP_CONAN_BUILD_ADDITIONALS%" ^
  -o "&:with_castor=False" ^
  -o "&:with_tests=%PEP_CONAN_BUILD_ADDITIONALS%" ^
  -o "&:with_benchmark=%PEP_CONAN_BUILD_ADDITIONALS%" ^
  -o "&:custom_build_folder=True" ^
  --output-folder=.\%BUILD_DIR%\ ^
  || exit /B 1

REM Put windeployqt and cmake in path
call .\generators\conanbuild.bat || exit /B 1

echo Configuring CMake project.
cmake --preset conan-default %PEP_CMAKE_DEFINES% || exit /B 1

echo Building.
cmake --build --preset conan-%PEP_CONAN_PRESET_FLAVOR% %PEP_CMAKE_BUILD_TARGETS% || exit /B 1

if [%WhatToBuild%] NEQ [wixlib] (
  goto :Done
)

cd .\%BUILD_DIR%\ || exit /B 1

REM The pepAssessor.exe used to be built in(to) the gui\Release subdirectory,
REM and (CI) logic and scripting is/was based on that. This script retains
REM backward compatibility by copying its stuff into that directory. That way
REM other scripts don't have to be updated (at this point).
REM TODO: use a proper staging directory instead.
mkdir gui || exit /B 1
mkdir gui\%BuildType% || exit /B 1

xcopy cpp\pep\assessor\%BuildType%\* gui\%BuildType% /s /e /y || exit /B 1
copy cpp\pep\logon\%BuildType%\pepLogon.exe gui\%BuildType% || exit /B 1
copy cpp\pep\cli\%BuildType%\pepcli.exe gui\%BuildType% || exit /B 1

echo Invoking WiX installer creation script.
call ..\installer\createBinWixLib.bat . %BuildType% || exit /B 1
call .\generators\deactivate_conanbuild.bat || exit /B 1

:Done
echo Finished CI job script for Windows build.
exit 0

:Invocation
echo Usage: %0 wixlib      [Build-Type]
echo    or: %0 all-targets [Build-Type]
echo where [Build-Type] must be either "Debug" or "Release". Defaults to "Release" if unspecified.
exit /B 1
