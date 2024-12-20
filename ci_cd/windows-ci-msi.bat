@echo off
set OwnDir=%~dp0

REM Ensure relative paths are interpreted w.r.t. this file's %OwnDir%
REM instead of the %OwnDir% that may be set by other (invoked) batch files.
set createWixInstaller=%OwnDir%\..\installer\createWixInstaller.bat
set createInstallerMetaXml=%OwnDir%\..\installer\createInstallerMetaXml.sh

echo Invoking Windows CI runner provisioning script.
call "%OwnDir%\windows-ci-runner.bat" || exit /B 1

REM Set BUILD_DIR to BUILD_DIRECTORY if defined, otherwise default to "build"
if "%BUILD_DIRECTORY%" == "" (
  set BUILD_DIR=build
) else (
  set BUILD_DIR=%BUILD_DIRECTORY%
)

call "%createWixInstaller%" %BUILD_DIR% pepBinaries.wixlib config %CI_COMMIT_REF_NAME% %CI_PIPELINE_ID% %CI_JOB_ID%|| exit /B 1

echo Staging WiX installer.
mkdir "%BUILD_DIR%\wix\msi"
copy "%BUILD_DIR%\wix\pep.msi" "%BUILD_DIR%\wix\msi\" || exit /B 1

"%createInstallerMetaXml%" %MAJOR_VERSION% %MINOR_VERSION% %CI_PIPELINE_ID% %CI_JOB_ID% "%BUILD_DIR%/wix/msi/" || exit /B 1
