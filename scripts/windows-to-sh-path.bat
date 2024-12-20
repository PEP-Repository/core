@echo off

setlocal
set OwnDir=%~dp0
set windowsPath=%1

if "%windowsPath%" == "" (
  set windowsPath=.
)

pushd "%windowsPath%"
call "%OwnDir%\invoke-sh.bat" pwd
popd
