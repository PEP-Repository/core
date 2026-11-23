@echo off
REM TODO: rewrite in PowerShell
REM Having this code in a batch file (as opposed to in .gitlab-ci.yml) allows it to be run and debugged locally.
REM Variables are prefixed with "PEP" to reduce chances of naming collisions.

echo Starting CI job script for Windows build
REM Colored output for e.g. Conan
set CLICOLOR_FORCE=1

REM Isolate this build from other builds running concurrently on the same runner (under the same user account).
REM Otherwise, when one CI job ends (or is cancelled or times out), the runner kills its process tree,
REM which may include helper processes that a concurrent job is still using.
REM See https://gitlab.pep.cs.ru.nl/pep/ops/-/work_items/293#note_54996.
REM This is only done for CI builds, so that local (incremental) builds can still benefit from the shared processes.
if not "%CI_JOB_ID%" == "" (
  REM MSBuild by default leaves worker nodes running after a build, and later builds may reuse them,
  REM  including concurrent builds in other CI jobs. When such a node is killed, MSBuild fails with
  REM  "error MSB4166: Child node [...] exited prematurely".
  REM  See https://github.com/dotnet/msbuild/blob/main/documentation/wiki/MSBuild-Environment-Variables.md
  REM  and -nodeReuse at https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference
  set MSBUILDDISABLENODEREUSE=1

  REM With /Zi, which is the default debug information format for CMake MSVC builds, cl.exe writes PDB files via MSPDBSRV.EXE.
  REM  By default, a single instance is shared by all builds of the same user. When it is killed, cl.exe fails with
  REM  "error C1090: PDB API call failed, error code '23': (0x00000006)".
  REM  The undocumented _MSPDBSRV_ENDPOINT_ environment variable selects which MSPDBSRV.EXE instance is used,
  REM  so we give each CI job its own instance.
  REM  See https://issues.jenkins.io/browse/JENKINS-9104 for the same problem with Jenkins,
  REM  and https://learn.microsoft.com/en-us/cpp/build/reference/fs-force-synchronous-pdb-writes for MSPDBSRV.EXE.
  set _MSPDBSRV_ENDPOINT_=pep-ci-job-%CI_JOB_ID%
)

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
pwsh -ExecutionPolicy Bypass -File ".\docker-build\scripts\windows-ci-conan.ps1" ^
  install .\docker-build\builder\conan\conanfile.py ^
  --lockfile=.\docker-build\builder\conan\conan-ci.lock ^
  --profile__all=.\docker-build\builder\conan\conan_profile ^
  --build=missing ^
  -s__a build_type="%BuildType%" ^
  -o "&:with_assessor=True" ^
  -o "&:with_servers=%PEP_CONAN_BUILD_ADDITIONALS%" ^
  -o "&:with_castor=%PEP_CONAN_BUILD_ADDITIONALS%" ^
  -o "&:with_tests=%PEP_CONAN_BUILD_ADDITIONALS%" ^
  -o "&:with_benchmark=%PEP_CONAN_BUILD_ADDITIONALS%" ^
  -o "&:custom_build_folder=True" ^
  --output-folder=.\%BUILD_DIR%\ ^
  || exit /B 1

REM Put windeployqt and cmake in path
call .\%BUILD_DIR%\generators\conanbuild.bat || exit /B 1

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
