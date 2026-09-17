param(
    [int]$PairsPerAnchor = 12,
    [int]$Concurrency = 0,
    [string]$StockfishTC = '10+0.1',
    [int[]]$Anchors = @(1600, 1800, 2000, 2200, 2400, 2600, 2800, 3000),
    [long]$Seed = 20260917
)

$ErrorActionPreference = 'Stop'
$TournamentDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FastChess = Join-Path $TournamentDir 'bin\fastchess.exe'
$Pickle = Join-Path $TournamentDir 'bin\pickle.exe'
$Stockfish = Join-Path $TournamentDir 'bin\stockfish.exe'
$Book = Join-Path $TournamentDir 'books\UHO_Lichess_4852_v1.epd'

foreach ($path in @($FastChess, $Pickle, $Stockfish, $Book)) {
    if (-not (Test-Path $path)) {
        throw "Missing tournament dependency: $path`nRun .\setup_windows.ps1 first."
    }
}

if ($Concurrency -le 0) {
    $Concurrency = [Math]::Max(1, [Math]::Min(6, [int]([Environment]::ProcessorCount / 2)))
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$ResultDir = Join-Path $TournamentDir "results\bracket-$timestamp"
New-Item -ItemType Directory -Force -Path $ResultDir | Out-Null
$Pgn = Join-Path $ResultDir 'games.pgn'
$Log = Join-Path $ResultDir 'fastchess.log'

$args = @(
    '-tournament', 'gauntlet',
    '-seeds', '1',
    '-engine', "cmd=$Pickle", 'name=Pickle_D11', 'depth=11', 'option.Hash=32', 'option.OwnBook=false'
)

foreach ($elo in $Anchors) {
    if ($elo -lt 1320 -or $elo -gt 3190) {
        throw "Stockfish UCI_Elo anchor $elo is outside the supported calibration range used by this harness (1320-3190)."
    }
    $args += @(
        '-engine', "cmd=$Stockfish", "name=Stockfish_$elo", "tc=$StockfishTC",
        'option.Threads=1', 'option.Hash=32', 'option.UCI_LimitStrength=true', "option.UCI_Elo=$elo"
    )
}

$args += @(
    '-openings', "file=$Book", 'format=epd', 'order=random',
    '-srand', "$Seed",
    '-rounds', "$PairsPerAnchor",
    '-repeat',
    '-concurrency', "$Concurrency",
    '-recover',
    '-ratinginterval', '20',
    '-report', 'penta=true',
    '-resign', 'movecount=3', 'score=600', 'twosided=true',
    '-draw', 'movenumber=34', 'movecount=8', 'score=20',
    '-pgnout', "file=$Pgn", 'notation=san', 'nodes=true', 'nps=true', 'append=false',
    '-log', "file=$Log", 'level=info', 'append=false', 'engine=false',
    '-event', 'Pickle D11 Elo bracket',
    '-site', 'local'
)

Write-Host "Quick bracket: $($Anchors -join ', ')" -ForegroundColor Cyan
Write-Host "Pairs per anchor: $PairsPerAnchor | Stockfish TC: $StockfishTC | Concurrency: $Concurrency"
Write-Host 'Pickle: fixed depth 11, OwnBook=false. All games use the same neutral UHO opening suite.'
Write-Host "Results: $ResultDir"
Write-Host ''

& $FastChess @args
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "FastChess exited with code $exitCode. See $Log"
}

$python = Get-Command python -ErrorAction SilentlyContinue
if ($python) {
    & python (Join-Path $TournamentDir 'estimate_elo.py') $Pgn --label 'Quick bracket (non-final TC)' | Tee-Object -FilePath (Join-Path $ResultDir 'elo-summary.txt')
} else {
    Write-Warning 'Python was not found, so the Elo summary was not generated automatically.'
}

Write-Host ''
Write-Host 'Bracket complete. Use the result to choose the center for run_calibration.ps1.' -ForegroundColor Green
