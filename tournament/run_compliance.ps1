$ErrorActionPreference = 'Stop'
$TournamentDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FastChess = Join-Path $TournamentDir 'bin\fastchess.exe'
$Pickle = Join-Path $TournamentDir 'bin\pickle.exe'

if (-not (Test-Path $FastChess) -or -not (Test-Path $Pickle)) {
    throw 'Tournament binaries are missing. Run .\setup_windows.ps1 first.'
}

Write-Host 'Running FastChess UCI compliance check for Pickle...'
& $FastChess --compliance $Pickle
if ($LASTEXITCODE -ne 0) {
    throw "FastChess compliance check failed with exit code $LASTEXITCODE."
}

Write-Host 'Pickle passed the FastChess UCI compliance check.' -ForegroundColor Green
