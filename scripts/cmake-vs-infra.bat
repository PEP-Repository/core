@echo off
REM CMake frontend to generate Visual Studio project files for a PEP (customer) project repository

REM Prevent this invocation's (environment) variables from being picked up by the next invocation
SETLOCAL

SET ProjectRoot=%1
SET InfraDirName=%2
SET CastorApiKeyDir=%3
SET BuildType=%4
SET ConanArgs=%5
SET CMakeArgs=%6

if %ProjectRoot%.==. GOTO :Invocation
if %InfraDirName%.==. GOTO :Invocation

SET OwnDir=%~dp0
SET InfraDir=%ProjectRoot%\config\%InfraDirName%

CALL %OwnDir%\cmake-vs.bat %ProjectRoot% %CastorApiKeyDir% %InfraDir% %InfraDir%\project %BuildType% %VsVersion% %ConanArgs% %CMakeArgs%

IF ERRORLEVEL 1 GOTO :End
IF ERRORLEVEL 0 exit /B 1
IF ERRORLEVEL -1 GOTO :End

:Invocation
@echo Invoke to produce Visual Studio project files for a customer's PEP infrastructure.
@echo Usage: %0 ^<project-root^> ^<infra-dir-name^> [CastorAPIKey-json-dir] [buildtype] [Conan args] [CMake args]
@echo   E.g. %0 .. blah_acc C:\MySecrets Release
exit /B 1

:End
