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

function EnterString {
  param ([string]$Prompt)
  
  $form = New-Object System.Windows.Forms.Form -Property @{
	Padding = 5
  }
  $panel1 = New-Object System.Windows.Forms.TableLayoutPanel -Property @{
	RowCount = 3
	ColumnCount = 1
	Padding = 5
	Dock = 'fill'
  }
  $form.controls.Add($panel1)
  
  $label1 = New-Object System.Windows.Forms.Label -Property @{
	Text = $Prompt
	Dock = 'fill'
  }
  $textbox1 = New-Object System.Windows.Forms.TextBox -Property @{
	Dock = 'fill'
  }
  $panel2 = New-Object System.Windows.Forms.TableLayoutPanel -Property @{
	RowCount = 1
	ColumnCount = 3
	Padding = 5
	Dock = 'fill'
  }
  $panel1.controls.Add($label1)
  $panel1.controls.Add($textbox1)
  $panel1.controls.Add($panel2)
  
  $panel3 = New-Object System.Windows.Forms.Panel -Property @{
	# Just something invisible to fill the first cell
	Dock = 'fill'
  }
  $btnOk = New-Object System.Windows.Forms.Button -Property @{
	Text = "OK"
	DialogResult = 'ok'
  }
  $btnCancel = New-Object System.Windows.Forms.Button -Property @{
	Text = "Cancel"
	DialogResult = 'cancel'
  }
  $panel2.controls.Add($panel3)
  $panel2.controls.Add($btnOk)
  $panel2.controls.Add($btnCancel)
  
  $stretchStyle = New-Object System.Windows.Forms.ColumnStyle -Property @{
	SizeType = 'Percent'
	Width = 100
  }
  $okStyle = New-Object System.Windows.Forms.ColumnStyle -Property @{
	SizeType = 'AutoSize'
  }
  $cancelStyle = New-Object System.Windows.Forms.ColumnStyle -Property @{
	SizeType = 'AutoSize'
  }
  $panel2.ColumnStyles.Clear();
  $panel2.ColumnStyles.Add($stretchStyle) | Out-Null
  $panel2.ColumnStyles.Add($okStyle) | Out-Null
  $panel2.ColumnStyles.Add($cancelStyle) | Out-Null

  $form.Width = 420
  $form.Height = 180
  $form.AcceptButton = $btnOk
  if ($form.ShowDialog() -eq 'OK') {
    return $textbox1.Text
  }
  else {
    Write-Output Cancelled
    exit 1
  }
}

function EnterNonEmptyString {
  param ([string]$Prompt)
  
  do {
    $result = EnterString $Prompt
	if (!$result) { ShowNotification 'Please enter a non-empty value.' }
  }
  while (!$result)
  return $result
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

  Write-Output 'Enter column name into dialog'
  # Note: dialog style & available properties differ between pwsh & Windows powershell
  $column = EnterNonEmptyString 'Column to store file into:'
 
  Write-Output 'Enter pseudonym into dialog'
  $pseud = EnterNonEmptyString 'Pseudonym of subject for which to store the file:'
  
  $listArgs = @(
    'list'
    '--metadata'
    '--local-pseudonyms'
    '--no-inline-data'
	'-p'
	  "$pseud"
	'-c'
	  "$column"
  )
  Write-Output "Running pepcli list to determine current cell contents"
  $ret = Start-Process pepcli $listArgs -WorkingDirectory $pepWorkingDirectory -NoNewWindow -Wait -PassThru
  if ($ret.ExitCode -ne 0) {
    ShowPepError "An error occurred while determining the current cell contents." pepcli
    pause
    exit $ret.ExitCode
  }


#  $browser = New-Object System.Windows.Forms.FolderBrowserDialog -Property @{
#    Description = "Select folder for PEP $projectCaption $reference download"
#    SelectedPath  = Join-Path $env:USERPROFILE Downloads
#  }
#  ShowNotification 'Please select the folder to place the downloaded data in, or select an existing download'
#  # Try to open dialog on foreground, but this may still fail
#  $choice = $browser.ShowDialog(
#    (New-Object System.Windows.Forms.Form -Property @{TopMost = $true; TopLevel = $true }))
#  if ($choice -eq [DialogResult]::Cancel) { exit 1 }
#  $folder = $browser.SelectedPath
#  $browser.Dispose()
#  if (!$folder) { exit 1 }
#
#  # If this is not an existing download folder, use a subfolder
#  if (!(Get-Item -ErrorAction Ignore (Join-Path $folder 'pepData.specification.json'))) {
#    $folder = Join-Path $folder "pulled-data-$projectCaption-$reference"
#  }
#
#  $pullArgs = @()
#
#  # If the pending folder was selected, strip the suffix
#  if ($folder.EndsWith('-pending')) {
#    $folder = $folder.Substring(0, $folder.Length - '-pending'.Length)
#  }
#
#  Get-Item -ErrorAction Ignore (Join-Path $folder 'pepData.specification.json')
#  $downloadPresent = $?
#
#  Get-Item -ErrorAction Ignore (Join-Path "$folder-pending" 'pepData.specification.json')
#  $partialDownloadPresent = $?
#
#  if ($partialDownloadPresent) {
#    $msg = "Folder $folder-pending apparently already contains a partial download.`n"
#    $msgFolder = 'folder'
#    if ($downloadPresent) {
#      $msg += "Which seems to have been created while updating an existing download in $folder.`n"
#      $msgFolder += 's'
#    }
#    $msg += "To resume the download, press OK.`n" +
#      "Press Cancel and remove the $msgFolder to initiate a fresh download."
#
#    $choice = [MessageBox]::Show(
#      $msg,
#      'Resume partial download?',
#      [MessageBoxButtons]::OKCancel,
#      [MessageBoxIcon]::Question)
#    if ($choice -eq [DialogResult]::Cancel) { exit 1 }
#    $pullArgs += @('--update'; '--resume')
#
#  } elseif ($downloadPresent) {
#    $choice = [MessageBox]::Show(
#      "Folder $folder apparently already contains a completed download.`n" +
#      'Do you want to update the existing download?',
#      'Update existing download?',
#      [MessageBoxButtons]::OKCancel,
#      [MessageBoxIcon]::Question)
#    if ($choice -eq [DialogResult]::Cancel) { exit 1 }
#    $pullArgs += '--update'
#
#  }
#
#  $choice = [MessageBox]::Show(
#    "Data will be downloaded to $folder, this may take a while. We'll notify you when it's done.`n" +
#    'Press OK to continue',
#    $null,
#    [MessageBoxButtons]::OKCancel,
#    [MessageBoxIcon]::Information)
#  if ($choice -eq [DialogResult]::Cancel) { exit 1 }
#
#  if ($pullArgs.Count -eq 0) {
#    $pullArgs = @('--all-accessible')
#  }
#  $pullArgs = @(
#    'pull'
#    '--report-progress'
#    '--output-directory'
#    $folder
#  ) + $pullArgs
#
#  Write-Output 'Download'
#  $ret = Start-Process pepcli $pullArgs -WorkingDirectory $pepWorkingDirectory -NoNewWindow -Wait -PassThru
#  if ($ret.ExitCode -ne 0) {
#    ShowPepError "An error occurred while downloading." pepcli
#    $retry = $false
#    if ($downloadPresent -or $partialDownloadPresent) {
#      $choice = [MessageBox]::Show(
#        "Do you want to retry the download but enable overwriting any local changes?`n" +
#        'Depending on the error, this may or may not work.',
#        'Force download?',
#        [MessageBoxButtons]::YesNo,
#        [MessageBoxIcon]::Warning)
#      $retry = $choice -eq [DialogResult]::yes
#    }
#    if (!$retry) {
#      pause
#      exit $ret.ExitCode
#    }
#    if ($partialDownloadPresent) {
#      $pullArgs = $pullArgs | where { $_ -ne '--resume' } # Remove incompatible flag
#      if (!$downloadPresent) {
#        # Remove --update when --resume was present and we don't have a base download
#        $pullArgs = $pullArgs | where { $_ -ne '--update' }
#        # Since no specification remains, specify what data we want
#        $pullArgs += '--all-accessible'
#      }
#    }
#    $pullArgs += '--force'
#    $ret = Start-Process pepcli $pullArgs -WorkingDirectory $pepWorkingDirectory -NoNewWindow -Wait -PassThru
#    if ($ret.ExitCode -ne 0) {
#      ShowPepError "An error occurred while retrying the download." pepcli
#      pause
#      exit $ret.ExitCode
#    }
#  }
#
#  ShowNotification 'Download complete!'
#  explorer "$folder"
}
catch { # Exception is in $_
  # Extra try-catch in case dialog throws
  try { ShowError "Unexpected error occurred:`n$_" } catch {}
  Write-Error $_ -ErrorAction Continue
  pause
  exit 1
}
