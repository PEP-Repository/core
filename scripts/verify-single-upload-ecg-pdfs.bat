@echo off

SET RootDir=%1
if %RootDir%.==. GOTO :Invocation

rem Ensure errorlevel is initialized to 0
ver > nul

rem Add some sanity to variable expansion within FOR loop: see https://stackoverflow.com/a/5615355
SETLOCAL ENABLEDELAYEDEXPANSION

SET ExitCode=0

rem Iterate over subdirectories of %RootDir%.
FOR /D %%d IN ("%RootDir%\*") DO (
	SET EcgSp=%%~nxd
	SET PdfFile=
	
	rem Iterate over *.pdf files in participant directory.
	FOR %%f IN ("%%d\*.pdf") DO (
		rem Check if previous iteration uploaded a file from this directory
		if not !PdfFile!.==. (
			@echo Directory "!EcgSp!" has multiple PDF files: "!PdfFile!" and "%%f"
			SET ExitCode=1
		)
		SET PdfFile=%%f
	)
)

goto :End

:Invocation
@echo Verifies that subdirectories contain a single ECG PDF file to upload to PEP.
@echo Usage: %0 ^<root-dir^>
@echo        where ^<root-dir^> contains subdirectories named after the ECG short pseudonym.
@echo   E.g. %0 "c:\ECGs\Visit 1\Beoordeeld"
goto :End

:End
EXIT /B %ExitCode%
