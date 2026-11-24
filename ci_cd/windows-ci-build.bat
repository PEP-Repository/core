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

REM Limit build parallelism, based on the number of CPUs and the amount of RAM, see windows-build-parallelism.ps1.
REM By default, MSBuild builds -maxCpuCount projects concurrently, and the compiler runs /MP processes for each project,
REM  so the number of compiler processes multiplies, and concurrent CI jobs on the same machine multiply it further.
REM  See https://learn.microsoft.com/en-us/cpp/build/reference/mp-build-with-multiple-processes#the-msbuild-tool
REM  In Visual Studio, -maxCpuCount is "Tools > Options > Projects and Solutions > Build and Run > Maximum number of parallel project builds".
REM Instead, we use MSBuild's Multi-ToolTask (MTT) scheduler, see https://devblogs.microsoft.com/cppblog/improved-parallelism-in-msbuild/
REM - UseMultiToolTask=true: MSBuild itself starts one cl.exe per source file, at most MultiProcMaxCount at a time.
REM   The /MP compiler option is then dropped.
REM - EnforceProcessCountAcrossBuilds=true: the MultiProcMaxCount limit is enforced across all projects and all MSBuild
REM   instances, and therefore across concurrent CI jobs on the same machine.
REM   Because of this, MultiProcMaxCount is based on the resources of the whole machine and should be the same for all builds on it.
REM   This only works with UseMSBuildResourceManager=false, see Directory.Build.props.
REM These are MSBuild properties that default to the environment variables of the same name.
REM  For their definitions, see Microsoft.Cpp.Common.props and Microsoft.CppCommon.targets in the Visual Studio installation.
REM  In Visual Studio, MultiProcMaxCount defaults to CL_MPCount from "Tools > Options > Projects and Solutions > VC++ Project Settings > Maximum concurrent C++ compilations"
REM This also applies to dependencies that Conan builds using MSBuild.
REM Linking is not limited by MTT, but only by the number of projects that MSBuild builds concurrently,
REM  which we pass to Conan as tools.microsoft.msbuild:max_cpu_count below.
set UseMultiToolTask=true
set EnforceProcessCountAcrossBuilds=true
for /f "usebackq tokens=1,* delims==" %%A in (`pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0windows-build-parallelism.ps1"`) do set "%%A=%%B"
if "%MultiProcMaxCount%" == "" (
  echo Could not determine build parallelism.
  exit /B 1
)
if "%PEP_MSBUILD_MAX_CPU_COUNT%" == "" (
  echo Could not determine build parallelism.
  exit /B 1
)
echo Using at most %MultiProcMaxCount% compiler processes and %PEP_MSBUILD_MAX_CPU_COUNT% concurrent MSBuild projects.

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
REM tools.microsoft.msbuild:max_cpu_count sets MSBuild's -maxCpuCount for dependencies that Conan builds,
REM  and for our own build via the generated CMake build preset (since Conan 2.34, see https://github.com/conan-io/conan/pull/20360).
REM  Without it, MSBuild builds only one project at a time.
REM tools.build:jobs is used for dependencies that Conan builds using other build systems (e.g. Ninja),
REM  which are not limited by MultiProcMaxCount. For Visual Studio generators, Conan also uses it for /MP, but MTT drops that.
REM See https://docs.conan.io/2/reference/config_files/global_conf.html
pwsh -ExecutionPolicy Bypass -File ".\docker-build\scripts\windows-ci-conan.ps1" ^
  install .\docker-build\builder\conan\conanfile.py ^
  --lockfile=.\docker-build\builder\conan\conan-ci.lock ^
  --profile__all=.\docker-build\builder\conan\conan_profile ^
  --build=missing ^
  -s__a build_type="%BuildType%" ^
  -c__a "tools.microsoft.msbuild:max_cpu_count=%PEP_MSBUILD_MAX_CPU_COUNT%" ^
  -c__a "tools.build:jobs=%MultiProcMaxCount%" ^
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
REM Note: we deliberately build the default target (Build), not the separate build passes
REM  /t:BuildGenerateSources;BuildCompile;BuildLink (with BuildPassReferences) from
REM  https://devblogs.microsoft.com/cppblog/cpp-build-throughput-investigation-and-tune-up/
REM  `cmake --build` builds ALL_BUILD.vcxproj, a Utility project that cannot take part in those passes, so it builds its
REM  references with their default target and executables then link before the static libraries they need exist (LNK1181).
REM  Building the passes on the solution (pep.slnx) does work, but measured at most ~7% faster in Release and slower in Debug.
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
