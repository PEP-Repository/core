@echo off

REM Wrapper to forward environment variables until everything is PowerShell
for /f "tokens=*" %%i in ('pwsh -ExecutionPolicy Bypass -File "%~dp0\..\scripts\wrap-echo-cmd-envvars.ps1" ^
  -ScriptPath "%~dp0\windows-ci-find-msvc.ps1" ^
  PEP_VISUAL_STUDIO_DIR PEP_VCVARS_BAT') do (

  echo Setting envvar: %%i
  %%i
)
