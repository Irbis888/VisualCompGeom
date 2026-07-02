$ErrorActionPreference = 'Stop'

$executable = Join-Path $PSScriptRoot 'build\Debug\geometry_visualizer.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $executable
