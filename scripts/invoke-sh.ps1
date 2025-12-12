param(
  [Parameter(Mandatory=$true, Position=0, ValueFromRemainingArguments=$true)]
  [string[]] $ScriptAndArgs
)

# First try Git Bash, then find in path, as Git bash is by default not in Path
# and to prevent using WSL bash
$bash = Get-Command "$env:ProgramFiles/Git/bin/bash" -ErrorAction SilentlyContinue
if (!$bash) {
  $bash = Get-Command "${env:ProgramFiles(x86)}/Git/bin/bash" -ErrorAction SilentlyContinue
}
if (!$bash) {
  $bash = 'bash'
}
# We use run.sh to support non-script commands (e.g. echo) and honor shebangs for scripts, without using `-c`,
# which would require us to escape arguments here
&$bash $PSScriptRoot/run.sh @ScriptAndArgs
exit $LASTEXITCODE
