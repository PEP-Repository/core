@echo off
REM Having this code in a batch file (as opposed to in .gitlab-ci.yml) allows it to be run and debugged locally.
REM Variables are prefixed with "PEP" to reduce chances of naming collisions.
set OwnDir=%~dp0

echo Starting CI job script for .wixobj creation.
REM Colored output for e.g. Conan
set CLICOLOR_FORCE=1

echo Invoking Windows CI runner provisioning script.
call "%OwnDir%\windows-ci-runner.bat" || exit /B 1

echo Initializing environment for Visual Studio tool invocation.

if [%PEP_VCVARS_BAT%] == [] (
  echo Windows CI runner provisioning script didn't provide the required PEP_VCVARS_BAT variable. Aborting.
  exit /B 1
)

call %PEP_VCVARS_BAT% || exit /B 1

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
conan install .\docker-build\builder\conan\conanfile.py ^
  --lockfile=.\docker-build\builder\conan\conan-ci.lock ^
  --profile:all=.\docker-build\builder\conan\conan_profile ^
  --build=missing ^
  -o "&:with_client=True" ^
  -o "&:with_servers=False" ^
  -o "&:with_castor=False" ^
  -o "&:with_tests=False" ^
  -o "&:with_benchmark=False" ^
  -o "&:custom_build_folder=True" ^
  --output-folder=.\%BUILD_DIR%\ ^
  || exit /B 1
if "%CLEAN_CONAN%" neq "" (
  echo Cleaning Conan cache.
  REM Remove some temporary build files (excludes binaries)
  conan remove "*" --lru 4w --confirm || exit /B 1
  REM Remove old packages
  conan cache clean --build --temp || exit /B 1
)

echo Configuring CMake project.
cmake --preset conan-default || exit /B 1

echo Building.
cmake --build --preset conan-release --target pepAssessor pepLogon pepcli || exit /B 1

cd .\%BUILD_DIR%\ || exit /B 1

REM The pepAssessor.exe used to be built in(to) the gui\Release subdirectory,
REM and (CI) logic and scripting is/was based on that. This script retains
REM backward compatibility by copying its stuff into that directory. That way
REM other scripts don't have to be updated (at this point).
REM TODO: use a proper staging directory instead.
mkdir gui || exit /B 1
mkdir gui\Release || exit /B 1

xcopy cpp\pep\assessor\Release\* gui\Release /s /e /y || exit /B 1
copy cpp\pep\logon\Release\pepLogon.exe gui\Release || exit /B 1
copy cpp\pep\cli\Release\pepcli.exe gui\Release || exit /B 1

echo Invoking WiX installer creation script.
REM Put windeployqt in path
call .\generators\conanbuild.bat || exit /B 1
call ..\installer\createBinWixLib.bat . Release || exit /B 1
call .\generators\deactivate_conanbuild.bat || exit /B 1

echo Finished CI job script for .wixlib creation.
