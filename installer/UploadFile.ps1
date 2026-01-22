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

  $host.ui.RawUI.WindowTitle = "PEP $projectCaption $reference one-click upload log"
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

  $browser = New-Object System.Windows.Forms.OpenFileDialog -Property @{
    Title = "Select file to upload"
    InitialDirectory  = Join-Path $env:USERPROFILE Downloads
  }
  # Try to open dialog on foreground, but this may still fail
  $choice = $browser.ShowDialog(
    (New-Object System.Windows.Forms.Form -Property @{TopLevel = $true }))
  if ($choice -eq [DialogResult]::Cancel) { exit 1 }
  $file = $browser.FileName
  $browser.Dispose()
  if (!$file) { exit 1 }

  $storeArgs = @(
    'store'
    '-p'
      "$pseud"
    '-c'
      "$column"
    '-i'
      "$file"
  )
  Write-Output 'Upload'
  $ret = Start-Process pepcli $storeArgs -WorkingDirectory $pepWorkingDirectory -NoNewWindow -Wait -PassThru
  if ($ret.ExitCode -ne 0) {
    ShowPepError "An error occurred while uploading." pepcli
  }
  else {
    ShowNotification "Upload completed"
  }
  
  # Remove keys after download: we rerun pepLogon anyway
  Remove-Item (Join-Path $pepWorkingDirectory 'ClientKeys.json')
}
catch { # Exception is in $_
  # Extra try-catch in case dialog throws
  try { ShowError "Unexpected error occurred:`n$_" } catch {}
  Write-Error $_ -ErrorAction Continue
  pause
  exit 1
}
