@echo off

setlocal
set OwnDir=%~dp0
set windowsPath=%1

if "%windowsPath%" == "" (
  set windowsPath=.
)

pushd "%windowsPath%"
powershell -ExecutionPolicy Bypass -File "%OwnDir%\invoke-sh.ps1" pwd
popd
