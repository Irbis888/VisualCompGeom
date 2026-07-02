$ErrorActionPreference = 'Stop'

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Visual Studio locator was not found: $vswhere"
}

$visualStudio = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $visualStudio) {
    throw 'Visual Studio with the C++ desktop toolchain was not found.'
}

$cmake = Join-Path $visualStudio `
    'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) {
    throw "Visual Studio CMake was not found: $cmake"
}

$buildDirectory = Join-Path $PSScriptRoot 'build'
$tempDirectory = Join-Path $buildDirectory '.tmp'
New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null
$env:TEMP = $tempDirectory
$env:TMP = $tempDirectory

& $cmake --fresh -S $PSScriptRoot -B $buildDirectory `
    -G 'Visual Studio 18 2026' -A x64

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $buildDirectory --config Debug
exit $LASTEXITCODE
