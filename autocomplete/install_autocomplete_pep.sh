#!/usr/bin/env bash
# System-wide install of PEP autocompletion for Linux
# Requires root privileges
set -e

declare -a pep_bins=(@cli_executables@)
# cli_executables will be replaced by CMake with all binary names

if [ "${pep_bins:0:1}" = "@" ]; then
  echo The installer should be processed by CMake first >&2
  exit 1
fi

# bash
if type pkg-config >/dev/null 2>&1; then
  bash_completions_dir="$(pkg-config --variable=completionsdir bash-completion 2>/dev/null)"
  if [ -n "$bash_completions_dir" ]; then
    cp "$(dirname -- "$0")/autocomplete_pep.bash" "$bash_completions_dir/${pep_bins[0]}"
    for bin in "${pep_bins[@]:1}"; do
      ln --force --symbolic "${pep_bins[0]}" "$bash_completions_dir/$bin"
    done
    echo Installed PEP completions for bash
  else
    echo bash-completion is not installed, will not install PEP completions for bash >&2
  fi
else
  echo pkg-config is not installed, will not install PEP completions for bash >&2
fi

# zsh
if type zsh >/dev/null 2>&1; then
  mkdir -p /usr/local/share/zsh/site-functions/
  cp "$(dirname -- "$0")/autocomplete_pep.zsh" /usr/local/share/zsh/site-functions/_pep
  echo Installed PEP completions for zsh
else
  echo zsh not found, will not install PEP completions for zsh >&2
fi

# pwsh
if type pwsh >/dev/null 2>&1; then
  pwsh_modules_dir=/usr/local/share/powershell/Modules/
  pwsh_module_name=AutocompletePep
  mkdir -p "$pwsh_modules_dir/$pwsh_module_name"
  cp "$(dirname -- "$0")/autocomplete_pep.ps1" "$pwsh_modules_dir/$pwsh_module_name/$pwsh_module_name.psm1"

  pwsh -c "
    if (!(Get-Module $pwsh_module_name)) {
      if (!(Test-Path \$PROFILE.AllUsersAllHosts)) {
        New-Item \$PROFILE.AllUsersAllHosts -ItemType File -Force >\$null
      }
      Add-Content \$PROFILE.AllUsersAllHosts 'Import-Module $pwsh_module_name'
    }
  "
  echo Installed PEP completions for pwsh
else
  echo pwsh not found, will not install PEP completions for pwsh >&2
fi
