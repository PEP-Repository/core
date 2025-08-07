@echo off
REM CMake frontend to generate Visual Studio project files for PEP

REM Prevent this invocation's (environment) variables from being picked up by the next invocation
SETLOCAL

SET OwnDir=%~dp0
SET PepFossRoot=%OwnDir%\..

SET ProjectRoot=%1
SET CastorApiKeyDir=%2
SET InfraDir=%3
SET ProjectDir=%4
SET BuildType=%5

REM If caller wants to specify CMakeArgs but no ConanArgs, they need to pass "" as ConanArgs.
REM The tilde drops the quotes so that we don't pass an empty parameter to Conan.
SET ConanArgs=%~6
SET CMakeArgs=%~7

if %ProjectRoot%.==. GOTO :Invocation

if %CastorApiKeyDir%.==. GOTO :SkipCastorApiKeySpecification
set CastorApiKeySpecification=-DCASTOR_API_KEY_DIR=%CastorApiKeyDir%
:SkipCastorApiKeySpecification

if %InfraDir%.==. GOTO :SkipInfrasDirSpecification
SET InfraDirSpecification=-DPEP_INFRA_DIR=%InfraDir%
:SkipInfrasDirSpecification

if %ProjectDir%.==. GOTO :SkipProjectDirSpecification
SET ProjectDirSpecification=-DPEP_PROJECT_DIR=%ProjectDir%
:SkipProjectDirSpecification

if %BuildType%.==. SET BuildType=Debug

if not "%QT_HOST_PATH%" == "" (
  echo QT_HOST_PATH environment variable should not be set as it will interfere with conan install
  echo Please deactivate any Conan environment (e.g. generators\deactivate_conanbuild.bat^) or open a new terminal
  goto :Error
)

REM Ensure that we're using the correct URL for conancenter: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2616#note_47704
conan remote update conancenter --url https://center2.conan.io

REM We set build_type for build requirements as well (with :a), because windeployqt copies its own dylibs,
REM  see https://github.com/conan-io/conan-center-index/issues/22693
conan install %PepFossRoot%\docker-build\builder\conan\conanfile.py ^
  --profile:all="%PepFossRoot%\docker-build\builder\conan\conan_profile" ^
  --build=missing ^
  -s:a build_type=%BuildType% ^
  -o "&:custom_build_folder=True" ^
  --output-folder=.\ ^
  %ConanArgs% ^
  || goto :Error

cmake %ProjectRoot% ^
  "-DCMAKE_TOOLCHAIN_FILE=.\generators\conan_toolchain.cmake" -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
  %InfraDirSpecification% %ProjectDirSpecification% %CastorApiKeySpecification% ^
  -DCMAKE_BUILD_TYPE=%BuildType% ^
  -DENABLE_TEST_DISCOVERY=on -DBUILD_GO_SERVERS=on ^
  %CMakeArgs% ^
  || goto :Error

REM At this point the build system has been successfully generated
goto :End

:Invocation
@echo Invoke to produce Visual Studio project files.
@echo Usage: %0 ^<project-root^> [CastorAPIKey-json-dir] [infra-dir] [project-dir] [buildtype] [Conan args] [CMake args]
@echo   E.g. %0 .. C:\MySecrets ..\config\gum_acc ..\config\projects\proj Release
exit /B 1

:Error
@echo Project file generation failed.
IF ERRORLEVEL 1 GOTO :End
IF ERRORLEVEL 0 exit /B 1
IF ERRORLEVEL -1 GOTO :End

:End
