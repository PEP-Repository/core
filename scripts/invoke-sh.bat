@echo off

REM Runs a *nix command line and makes its output and exit code available to the
REM invoking DOS environment. See the "results-to-files.sh" script for details.

REM Prevent this invocation's (environment) variables from being picked up by the next invocation
SETLOCAL

SET OwnDir=%~dp0

REM Get all arguments: https://stackoverflow.com/a/16354963
SET "allargs=%*"
IF "%allargs%" == "" (
  echo No command line provided 1>&2
  exit /B 1
)

%OwnDir%\results-to-files.sh %allargs%

if not exist result.txt (
  echo Invocation's exit code was not written to result.txt file
  exit /B 1
)

set /p result=<result.txt
del result.txt

if exist stdout.txt (
  type stdout.txt
  del stdout.txt
)

if exist stderr.txt (
  type stderr.txt 1>&2
  del stderr.txt
)

exit /B %result%
