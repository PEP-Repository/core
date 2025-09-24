@echo off
rem this script will remove all files, except json and bat files from the current directory, recursively.
rem this can be used to clean up a download directory from 'pepcli pull', after it has been copied to a different location, in a way that we can update it with the -u and --assume-pristine flags

for /r %%X in (*.*) do (
	if not "%%~xX" == ".json" if not "%%~xX" == ".bat" del %%X
)