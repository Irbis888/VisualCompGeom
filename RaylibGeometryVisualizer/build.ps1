param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

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

$installationVersion = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationVersion
$visualStudioMajor = ([version]$installationVersion).Major
$generator = switch ($visualStudioMajor) {
    18 { 'Visual Studio 18 2026' }
    17 { 'Visual Studio 17 2022' }
    default {
        throw "Unsupported Visual Studio major version: $visualStudioMajor"
    }
}

$cmake = Join-Path $visualStudio `
    'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) {
    throw "Visual Studio CMake was not found: $cmake"
}

$buildDirectory = Join-Path $PSScriptRoot 'build'
$staleCacheFound = $false
if (Test-Path -LiteralPath $buildDirectory) {
    $cacheFiles = Get-ChildItem -LiteralPath $buildDirectory `
        -Filter 'CMakeCache.txt' -File -Recurse -ErrorAction SilentlyContinue
    foreach ($cacheFile in $cacheFiles) {
        $homeEntry = Select-String -LiteralPath $cacheFile.FullName `
            -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$' | Select-Object -First 1
        if ($homeEntry -and -not (Test-Path -LiteralPath $homeEntry.Matches[0].Groups[1].Value)) {
            $staleCacheFound = $true
            break
        }
    }
}

if ($staleCacheFound) {
    $projectRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
    $resolvedBuildDirectory = [System.IO.Path]::GetFullPath($buildDirectory)
    if (-not $resolvedBuildDirectory.StartsWith(
        $projectRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean an unsafe build path: $resolvedBuildDirectory"
    }
    Write-Host 'Removing stale CMake build data from the previous repository location.'
    Remove-Item -LiteralPath $resolvedBuildDirectory -Recurse -Force
}

$tempDirectory = Join-Path $buildDirectory '.tmp'
New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null
$env:TEMP = $tempDirectory
$env:TMP = $tempDirectory

& $cmake -S $PSScriptRoot -B $buildDirectory -G $generator -A x64

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $buildDirectory --config $Configuration
exit $LASTEXITCODE
