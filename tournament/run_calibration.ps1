param(
    [int]$Center = 2400,
    [int]$Step = 100,
    [int]$Radius = 2,
    [int]$PairsPerAnchor = 20,
    [int]$Concurrency = 0,
    [string]$StockfishTC = '120+1.0',
    [long]$Seed = 20260918
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
    $Concurrency = [Math]::Max(1, [Math]::Min(4, [int]([Environment]::ProcessorCount / 2)))
}

$Anchors = @()
for ($i = -$Radius; $i -le $Radius; $i++) {
    $rating = $Center + ($i * $Step)
    if ($rating -ge 1320 -and $rating -le 3190) {
        $Anchors += $rating
    }
}
$Anchors = @($Anchors | Sort-Object -Unique)
if ($Anchors.Count -lt 2) {
    throw 'Calibration needs at least two valid Stockfish Elo anchors in the 1320-3190 range.'
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$ResultDir = Join-Path $TournamentDir "results\calibration-$timestamp"
New-Item -ItemType Directory -Force -Path $ResultDir | Out-Null
$Pgn = Join-Path $ResultDir 'games.pgn'
$Log = Join-Path $ResultDir 'fastchess.log'

$args = @(
    '-tournament', 'gauntlet',
    '-seeds', '1',
    '-engine', "cmd=$Pickle", 'name=Pickle_D11', 'depth=11', 'option.Hash=32', 'option.OwnBook=false'
)

foreach ($elo in $Anchors) {
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
    '-event', 'Pickle D11 Stockfish-UCI-Elo calibration',
    '-site', 'local'
)

Write-Host "Calibration anchors: $($Anchors -join ', ')" -ForegroundColor Cyan
Write-Host "Pairs per anchor: $PairsPerAnchor | Stockfish TC: $StockfishTC | Concurrency: $Concurrency"
Write-Host 'Pickle: fixed depth 11, OwnBook=false. Each opening is repeated with colors reversed.'
Write-Host 'This is the final calibration mode; it intentionally uses Stockfish current UCI_Elo calibration time control.'
Write-Host "Results: $ResultDir"
Write-Host ''

& $FastChess @args
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "FastChess exited with code $exitCode. See $Log"
}

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    throw 'Python is required to calculate the calibrated Elo estimate from the completed PGN.'
}

& python (Join-Path $TournamentDir 'estimate_elo.py') $Pgn --label 'Stockfish UCI_Elo calibration' | Tee-Object -FilePath (Join-Path $ResultDir 'elo-summary.txt')

Write-Host ''
Write-Host 'Calibration complete. The estimate and 95% bootstrap interval are in elo-summary.txt.' -ForegroundColor Green
