@echo off

REM Invoke PowerShell script, then pick up / copy any environment variables that it set (especially PEP_VCVARS_BAT)
for /f "tokens=*" %%i in ('pwsh -ExecutionPolicy Bypass -File "%~dp0\..\scripts\wrap-echo-cmd-envvars.ps1" ^
  -ScriptPath "%~dp0\windows-ci-find-msvc.ps1" ^
  PEP_VISUAL_STUDIO_DIR PEP_VCVARS_BAT') do (

  echo Setting envvar: %%i
  %%i
)
