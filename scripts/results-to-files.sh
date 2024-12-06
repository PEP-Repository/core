#!/usr/bin/env sh

# Invokes a command line and writes its output and exit code to files.
#
# Windows machines (can) use Git Bash to execute *nix shell scripts, but the
# script's exit code or output are then not made available to the invoking DOS
# environment (i.e. command prompt or batch file). This script helps by writing
# the *nix output and exit code to files, from where the (DOS) caller can then
# pick them up to perform further processing. The complementary "invoke-sh.bat"
# does so to allow DOS environments to run *nix scripts (mostly) transparently.

if [ "$#" -eq 0 ] ; then
  >&2 echo No command line specified
  exit 1
fi

# Redirect output streams to file and store exit code in variable
"$@" 2> stderr.txt 1> stdout.txt && result=$? || result=$?

# Write exit code to file
echo "$result" > result.txt
