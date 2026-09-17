param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$TournamentDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $TournamentDir
$BinDir = Join-Path $TournamentDir 'bin'
$BookDir = Join-Path $TournamentDir 'books'
$ResultsDir = Join-Path $TournamentDir 'results'
$DownloadDir = Join-Path $TournamentDir '.downloads'

foreach ($dir in @($BinDir, $BookDir, $ResultsDir, $DownloadDir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

function Invoke-DownloadFile {
    param([string]$Url, [string]$Destination)
    Write-Host "Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing
}

function Get-LatestRelease {
    param([string]$Repository)
    $headers = @{ 'User-Agent' = 'Pickle-Elo-Harness' }
    Invoke-RestMethod -Uri "https://api.github.com/repos/$Repository/releases/latest" -Headers $headers
}

function Select-ReleaseAsset {
    param($Release, [scriptblock]$Predicate, [string]$Description)
    $asset = $Release.assets | Where-Object $Predicate | Select-Object -First 1
    if (-not $asset) {
        throw "Could not find $Description in release $($Release.tag_name)."
    }
    return $asset
}

if (-not $SkipBuild) {
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw 'CMake is required to build Pickle. Install CMake and a Windows C++ toolchain, then rerun this script.'
    }

    $BuildDir = Join-Path $RepoRoot 'build-tournament'
    Write-Host 'Building Pickle in Release mode...'
    & cmake -S $RepoRoot -B $BuildDir -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & cmake --build $BuildDir --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Pickle build failed.' }

    $pickleCandidates = @(
        (Join-Path $BuildDir 'Release\pickle.exe'),
        (Join-Path $BuildDir 'pickle.exe')
    )
    $pickle = $pickleCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $pickle) {
        throw 'Build completed but pickle.exe was not found.'
    }
    Copy-Item $pickle (Join-Path $BinDir 'pickle.exe') -Force
}
elseif (-not (Test-Path (Join-Path $BinDir 'pickle.exe'))) {
    throw '-SkipBuild was requested but tournament\bin\pickle.exe does not exist.'
}

Write-Host 'Fetching latest FastChess Windows x86-64 release...'
$fastRelease = Get-LatestRelease 'Disservin/fastchess'
$fastAsset = Select-ReleaseAsset $fastRelease {
    $_.name -match '(?i)windows' -and $_.name -match '(?i)(x86[-_]?64|x64)' -and $_.name -match '\.zip$'
} 'a Windows x86-64 FastChess ZIP'
$fastZip = Join-Path $DownloadDir $fastAsset.name
Invoke-DownloadFile $fastAsset.browser_download_url $fastZip
$fastExtract = Join-Path $DownloadDir 'fastchess'
Remove-Item $fastExtract -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -Path $fastZip -DestinationPath $fastExtract -Force
$fastExe = Get-ChildItem $fastExtract -Recurse -File | Where-Object { $_.Name -match '(?i)^fastchess.*\.exe$' } | Select-Object -First 1
if (-not $fastExe) { throw 'FastChess ZIP did not contain a Windows executable.' }
Copy-Item $fastExe.FullName (Join-Path $BinDir 'fastchess.exe') -Force

Write-Host 'Fetching latest official Stockfish release...'
$sfRelease = Get-LatestRelease 'official-stockfish/Stockfish'
$sfAssets = @($sfRelease.assets)

# Stockfish 19 names its 64-bit Intel/AMD package
# stockfish-windows-x86-64-universal.zip. Older releases have also used x64 and
# feature-specific package names, so accept either spelling while rejecting ARM64.
$sfAsset = $sfAssets | Where-Object {
    $_.name -match '(?i)^stockfish-windows-(x86[-_]?64|x64).*\.zip$' -and
    $_.name -notmatch '(?i)arm64'
} | Sort-Object {
    if ($_.name -match '(?i)sse41') { 0 }
    elseif ($_.name -match '(?i)universal') { 1 }
    elseif ($_.name -match '(?i)avx2') { 2 }
    else { 3 }
} | Select-Object -First 1

if (-not $sfAsset) {
    throw "Could not find a compatible Windows x86-64 Stockfish ZIP in release $($sfRelease.tag_name). Assets: $((@($sfAssets.name) -join ', '))"
}
$sfZip = Join-Path $DownloadDir $sfAsset.name
Invoke-DownloadFile $sfAsset.browser_download_url $sfZip
$sfExtract = Join-Path $DownloadDir 'stockfish'
Remove-Item $sfExtract -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -Path $sfZip -DestinationPath $sfExtract -Force
$sfExe = Get-ChildItem $sfExtract -Recurse -File | Where-Object { $_.Name -match '(?i)^stockfish.*\.exe$' } | Select-Object -First 1
if (-not $sfExe) { throw 'Stockfish ZIP did not contain a Windows executable.' }
Copy-Item $sfExe.FullName (Join-Path $BinDir 'stockfish.exe') -Force

# Pin the opening suite so runs can be reproduced later.
$bookCommit = '65815ccdbc7727cd4f6aee252ba8f67fb740e92f'
$bookZip = Join-Path $DownloadDir 'UHO_Lichess_4852_v1.epd.zip'
$bookUrl = "https://raw.githubusercontent.com/official-stockfish/books/$bookCommit/UHO_Lichess_4852_v1.epd.zip"
Invoke-DownloadFile $bookUrl $bookZip
$bookExtract = Join-Path $DownloadDir 'book'
Remove-Item $bookExtract -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -Path $bookZip -DestinationPath $bookExtract -Force
$bookFile = Get-ChildItem $bookExtract -Recurse -File | Where-Object { $_.Name -eq 'UHO_Lichess_4852_v1.epd' } | Select-Object -First 1
if (-not $bookFile) { throw 'Opening suite ZIP did not contain UHO_Lichess_4852_v1.epd.' }
Copy-Item $bookFile.FullName (Join-Path $BookDir 'UHO_Lichess_4852_v1.epd') -Force

$picklePath = Join-Path $BinDir 'pickle.exe'
$fastPath = Join-Path $BinDir 'fastchess.exe'
$sfPath = Join-Path $BinDir 'stockfish.exe'
$bookPath = Join-Path $BookDir 'UHO_Lichess_4852_v1.epd'
$pickleCommit = 'unknown (repository was not available through git)'
if (Get-Command git -ErrorAction SilentlyContinue) {
    try {
        $resolvedCommit = (& git -C $RepoRoot rev-parse HEAD 2>$null)
        if ($LASTEXITCODE -eq 0 -and $resolvedCommit) { $pickleCommit = $resolvedCommit.Trim() }
    } catch { }
}

$metadata = @(
    "Created UTC: $([DateTime]::UtcNow.ToString('o'))",
    "CPU: $env:PROCESSOR_IDENTIFIER",
    "Pickle commit: $pickleCommit",
    "Pickle SHA256: $((Get-FileHash $picklePath -Algorithm SHA256).Hash)",
    "FastChess release: $($fastRelease.tag_name)",
    "FastChess asset: $($fastAsset.name)",
    "FastChess SHA256: $((Get-FileHash $fastPath -Algorithm SHA256).Hash)",
    "Stockfish release: $($sfRelease.tag_name)",
    "Stockfish asset: $($sfAsset.name)",
    "Stockfish SHA256: $((Get-FileHash $sfPath -Algorithm SHA256).Hash)",
    "Opening suite: official-stockfish/books@$bookCommit UHO_Lichess_4852_v1.epd",
    "Opening suite SHA256: $((Get-FileHash $bookPath -Algorithm SHA256).Hash)"
)
$metadata | Set-Content -Path (Join-Path $TournamentDir 'environment.txt') -Encoding utf8

Write-Host ''
Write-Host 'Tournament environment is ready:' -ForegroundColor Green
Write-Host "  Pickle:    $picklePath"
Write-Host "  FastChess: $fastPath"
Write-Host "  Stockfish: $sfPath"
Write-Host "  Openings:  $bookPath"
Write-Host ''
Write-Host 'Next: .\run_compliance.ps1, then .\run_bracket.ps1'
