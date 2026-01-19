using namespace System.Windows.Forms  # Avoid typing full names like [System.Windows.Forms.MessageBox]

[CmdletBinding()]
# Optional parameter for development
param (
  [Parameter(HelpMessage = 'Explicitly set PEP install path to find config & binaries')]
  [string]$InstallPath
)

if (!$InstallPath) {
  $InstallPath = $PSScriptRoot
}

$pepWorkingDirectory = Join-Path $env:APPDATA 'PEP'
$logsFolder = Join-Path $pepWorkingDirectory 'rotated_logs'

function ShowError {
  param ([string]$Message)
  $null = [MessageBox]::Show(
    $Message,
    'Error',
    [MessageBoxButtons]::OK,
    [MessageBoxIcon]::Error)
}

function ShowPepError {
  param ([string]$Message, [string]$Exe)
  Write-Output "Log folder: $logsFolder"
  ShowError ($Message + "`nyou may find additional info in the log window on the taskbar, or in $logsFolder for $Exe.`n" +
    'You could also try updating PEP by opening PEP Assessor or downloading the latest installer')
}

function ShowNotification {
  param ([string]$Message)
  $null = [MessageBox]::Show(
    $Message,
    $null,
    [MessageBoxButtons]::OK,
    [MessageBoxIcon]::Information)
}

$ErrorActionPreference = 'Stop'
try {
  Add-Type -AssemblyName System.Windows.Forms  # Load types MessageBox, FolderBrowserDialog, ...

  $config = Get-Content (Join-Path $InstallPath 'configVersion.json') | ConvertFrom-Json
  $projectCaption = $config.projectCaption
  $reference = $config.reference

  $host.ui.RawUI.WindowTitle = "PEP $projectCaption $reference one-click download log"
  $env:Path = "$InstallPath;$env:Path"

  pepcli --version
  Write-Output ''

  Write-Output 'Login: opening browser'
  $ret = Start-Process pepLogon -WorkingDirectory $pepWorkingDirectory -NoNewWindow -Wait -PassThru
  if ($ret.ExitCode -ne 0) {
    ShowPepError "An error occurred while logging you in." pepLogon
    pause
    exit $ret.ExitCode
  }
  Write-Output 'Login complete'

  Write-Output 'Select destination in dialog'
  # Note: dialog style & available properties differ between pwsh & Windows powershell
  $browser = New-Object System.Windows.Forms.FolderBrowserDialog -Property @{
    Description = "Select folder for PEP $projectCaption $reference download"
    SelectedPath  = Join-Path $env:USERPROFILE Downloads
  }
  ShowNotification 'Please select the folder to place the downloaded data in, or select an existing download'
  # Try to open dialog on foreground, but this may still fail
  $choice = $browser.ShowDialog(
    (New-Object System.Windows.Forms.Form -Property @{TopMost = $true; TopLevel = $true }))
  if ($choice -eq [DialogResult]::Cancel) { exit 1 }
  $folder = $browser.SelectedPath
  $browser.Dispose()
  if (!$folder) { exit 1 }

  # If this is not an existing download folder, use a subfolder
  if (!(Get-Item -ErrorAction Ignore (Join-Path $folder 'pepData.specification.json'))) {
    $folder = Join-Path $folder "pulled-data-$projectCaption-$reference"
  }

  $pullArgs = @()

  # If the pending folder was selected, strip the suffix
  if ($folder.EndsWith('-pending')) {
    $folder = $folder.Substring(0, $folder.Length - '-pending'.Length)
  }

  Get-Item -ErrorAction Ignore (Join-Path $folder 'pepData.specification.json')
  $downloadPresent = $?

  Get-Item -ErrorAction Ignore (Join-Path "$folder-pending" 'pepData.specification.json')
  $partialDownloadPresent = $?

  if ($partialDownloadPresent) {
    $msg = "Folder $folder-pending apparently already contains a partial download.`n"
    $msgFolder = 'folder'
    if ($downloadPresent) {
      $msg += "Which seems to have been created while updating an existing download in $folder.`n"
      $msgFolder += 's'
    }
    $msg += "To resume the download, press OK.`n" +
      "Press Cancel and remove the $msgFolder to initiate a fresh download."

    $choice = [MessageBox]::Show(
      $msg,
      'Resume partial download?',
      [MessageBoxButtons]::OKCancel,
      [MessageBoxIcon]::Question)
    if ($choice -eq [DialogResult]::Cancel) { exit 1 }
    $pullArgs += @('--update'; '--resume')

  } elseif ($downloadPresent) {
    $choice = [MessageBox]::Show(
      "Folder $folder apparently already contains a completed download.`n" +
      'Do you want to update the existing download?',
      'Update existing download?',
      [MessageBoxButtons]::OKCancel,
      [MessageBoxIcon]::Question)
    if ($choice -eq [DialogResult]::Cancel) { exit 1 }
    $pullArgs += '--update'

  }

  $choice = [MessageBox]::Show(
    "Data will be downloaded to $folder, this may take a while. We'll notify you when it's done.`n" +
    'Press OK to continue',
    $null,
    [MessageBoxButtons]::OKCancel,
    [MessageBoxIcon]::Information)
  if ($choice -eq [DialogResult]::Cancel) { exit 1 }

  if ($pullArgs.Count -eq 0) {
    $pullArgs = @('--all-accessible')
  }
  $pullArgs = @(
    'pull'
    '--report-progress'
    '--output-directory'
    $folder
  ) + $pullArgs

  Write-Output 'Download'
  $ret = Start-Process pepcli $pullArgs -WorkingDirectory $pepWorkingDirectory -NoNewWindow -Wait -PassThru
  if ($ret.ExitCode -ne 0) {
    $canForce = $downloadPresent -or $partialDownloadPresent
    $msg = 'An error occurred while downloading.'
    if ($canForce) {
      $msg += ' Press OK for options.'
    }
    ShowPepError $msg pepcli
    $retry = $false
    if ($canForce) {
      $choice = [MessageBox]::Show(
        "Do you want to retry the download but enable overwriting any local changes?`n" +
        'Depending on the error, this may or may not work.',
        'Force download?',
        [MessageBoxButtons]::YesNo,
        [MessageBoxIcon]::Warning)
      $retry = $choice -eq [DialogResult]::yes
    }
    if (!$retry) {
      pause
      exit $ret.ExitCode
    }
    $pullArgs += '--force'
    $ret = Start-Process pepcli $pullArgs -WorkingDirectory $pepWorkingDirectory -NoNewWindow -Wait -PassThru
    if ($ret.ExitCode -ne 0) {
      ShowPepError "An error occurred while retrying the download." pepcli
      pause
      exit $ret.ExitCode
    }
  }

  ShowNotification 'Download complete!'
  explorer "$folder"
}
catch { # Exception is in $_
  # Extra try-catch in case dialog throws
  try { ShowError "Unexpected error occurred:`n$_" } catch {}
  Write-Error $_ -ErrorAction Continue
  pause
  exit 1
}
