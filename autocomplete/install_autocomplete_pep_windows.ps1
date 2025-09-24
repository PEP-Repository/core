#!/usr/bin/env -S powershell -File
#Requires -PSEdition Desktop
# User install of PEP autocompletion for Windows
# Run this from Windows Powershell, not the new Powershell Core

$module_name = 'AutocompletePep'

# powershell
$modules_dir = "$([Environment]::GetFolderPath('MyDocuments'))\WindowsPowerShell\Modules"
New-Item -Type Directory -Force "$modules_dir/$module_name" >$null
Copy-Item "$PSScriptRoot/autocomplete_pep.ps1" "$modules_dir/$module_name/$module_name.psm1"

if (!(Get-Module $module_name)) {
if (!(Test-Path $PROFILE)) {
    New-Item $PROFILE -ItemType File -Force >$null
}
Add-Content $PROFILE "Import-Module $module_name"
}
Write-Output 'Installed PEP completions for powershell'

# pwsh
if (Get-Command pwsh -ErrorAction SilentlyContinue) {
  $pwsh_modules_dir = "$([Environment]::GetFolderPath('MyDocuments'))\PowerShell\Modules"
  New-Item -Type Directory -Force "$pwsh_modules_dir/$module_name" >$null
  Copy-Item "$PSScriptRoot/autocomplete_pep.ps1" "$pwsh_modules_dir/$module_name/$module_name.psm1"

  pwsh -c @"
    if (!(Get-Module $module_name)) {
      if (!(Test-Path `$PROFILE)) {
        New-Item `$PROFILE -ItemType File -Force >`$null
      }
      Add-Content `$PROFILE 'Import-Module $module_name'
    }
"@
  Write-Output 'Installed PEP completions for pwsh'
} else {
  Write-Error 'pwsh not found, will not install PEP completions for pwsh'
}
