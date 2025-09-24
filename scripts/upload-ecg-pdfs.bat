@echo off

SET EcgColumnName=%1
SET RootDir=%~2
SET IdFile=%~3
if "%IdFile%"=="" GOTO :Invocation

rem Ensure errorlevel is initialized to 0
ver > nul

rem Add some sanity to variable expansion within FOR and IF: see https://stackoverflow.com/a/5615355
SETLOCAL ENABLEDELAYEDEXPANSION

rem Call script that verifies only a single PDF file exists in each subdir.
set OwnDir=%~dp0
call "%OwnDir%\verify-single-upload-ecg-pdfs.bat" "%RootDir%"
if %errorlevel% neq 0 (
	@echo Terminating: please ensure that each subdirectory contains at most one PDF file.
	exit /B !errorlevel!
)

rem Iterate over lines in the ID file: https://stackoverflow.com/a/24928087
for /F "usebackq tokens=1-2 delims=," %%a in ("%IdFile%") do (
	SET ParticipantId=%%a
	SET EcgSp=%%b
	rem Somehow %RootDir% is quoted if I don't suppress quotes explicitly
	SET DataDir=%RootDir:"=%\!EcgSp!
	@echo Processing directory "!DataDir!" for participant "!ParticipantId!".
	
	if exist "!DataDir!" (
		rem Iterate over PDF files in data directory.
		FOR %%f IN ("!DataDir!\*.pdf") DO (
			SET CliInvocation=pepcli store --participant "!ParticipantId!" --column %EcgColumnName% --input-file "%%f"
			rem Remove the "@echo" when you want to run for real
			@echo     !CliInvocation!
			
			rem Terminate if pepcli fails
			if !errorlevel! neq 0 (
				@echo Terminating: "pepcli store" failed
				exit /B !errorlevel!
			)
			
			@echo     Uploaded "%%f"
		)
	)
)
goto :End
	  
:Invocation
@echo Uploads ECG PDF files to PEP. Invoke from a directory containing pepcli(.exe).
@echo Usage:
@echo     %0 ^<column-name^> ^<root-dir^> ^<id-file^>
@echo - ^<column-name^> is the name of the PEP column to store the PDFs in.
@echo - ^<root-dir^> contains subdirectories named after the ECG's short pseudonym,
@echo   and each subdirectory contains a maximum of 1 PDF file to upload.
@echo - ^<id-file^> is a comma-delimited file associating participant IDs with
@echo   ECG short pseudonyms.
@echo E.g.
@echo     %0 "Visit1.ECG" "c:\ECGs\Visit 1\Beoordeeld" "c:\Temp\Participant data.csv"
goto :End

:End
