using namespace System.Windows.Forms

function ShowError {
	param ([string]$Message)
	$null = [MessageBox]::Show(
		$Message,
		'Error',
		[MessageBoxButtons]::OK,
		[MessageBoxIcon]::Error)
}

$pepWorkingDirectory = Join-Path $env:APPDATA 'PEP'
$logsFolder = Join-Path $pepWorkingDirectory 'rotated_logs'

function ShowPepError {
	param ([string]$Message, [string]$Exe)
	Write-Output "Log folder: $logsFolder"
	ShowError ($Message + "`nyou may find additional info in the log window, or in $logsFolder for $Exe`n" +
		'You could try updating PEP by opening PEP Assessor or downloading the latest installer')
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
	$config = Get-Content (Join-Path $PSScriptRoot 'configVersion.json') | ConvertFrom-Json
	$projectCaption = $config.projectCaption
	$reference = $config.reference

	$host.ui.RawUI.WindowTitle = "PEP $projectCaption $reference one-click download log"
	$env:Path = "$PSScriptRoot;$env:Path"

	Write-Output 'Login: opening browser'
	$ret = Start-Process pepLogon -WorkingDirectory $pepWorkingDirectory -NoNewWindow -Wait -PassThru
	if ($ret.ExitCode -ne 0) {
		ShowPepError "An error occurred while logging you in." pepLogon
		pause
		exit $ret.ExitCode
	}
	Write-Output 'Login complete'

	Add-Type -AssemblyName System.Windows.Forms
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

	if (Get-Item -ErrorAction Ignore (Join-Path $folder 'pepData.specification.json')) {
		$choice = [MessageBox]::Show(
			"Folder $folder apparently already contains a completed download.`n" +
			'Do you want to update the existing download?',
			'Update existing download?',
			[MessageBoxButtons]::OKCancel,
			[MessageBoxIcon]::Question)
		if ($choice -eq [DialogResult]::Cancel) { exit 1 }
		$pullArgs += '--update'

	}
	elseif (Get-Item -ErrorAction Ignore (Join-Path "$folder-pending" 'pepData.specification.json')) {
		$choice = [MessageBox]::Show(
			"Folder $folder-pending apparently already contains a partial download.`n" +
			'Do you want to resume (retry) the download or ignore the partial download and perform a fresh one?',
			'Resume partial download?',
			[MessageBoxButtons]::AbortRetryIgnore,
			[MessageBoxIcon]::Question)
		switch ($choice) {
			[DialogResult]::Abort { exit 1 }
			[DialogResult]::Retry { $pullArgs += '--resume' }
		}
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
		ShowPepError "An error occurred while downloading." pepcli
		pause
		exit $ret.ExitCode
	}

	ShowNotification 'Download complete!'
	explorer "$folder"
}
catch {
	$ErrorActionPreference = 'Continue'
	ShowError "Unexpected error occurred:`n$_"
	Write-Error $_
	pause
	exit 1
}
