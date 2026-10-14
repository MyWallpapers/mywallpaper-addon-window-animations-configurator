param([string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repo '.build/windows-x86_64' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$instances = @((& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json) | ConvertFrom-Json)
if ($LASTEXITCODE -or -not $instances.Count) { throw 'Visual Studio C++ x64 build tools required.' }
$instance = $instances[0]
$vs = $instance.installationPath
$generator = switch (([version]$instance.installationVersion).Major) {
    17 { 'Visual Studio 17 2022' }
    18 { 'Visual Studio 18 2026' }
    default { throw 'Unsupported Visual Studio version; update the supported CMake generator explicitly.' }
}
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
$pin = Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'dependencies.lock.json') | ConvertFrom-Json
$header = Join-Path $repo 'third_party/nlohmann/json.hpp'
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $headerHash = [BitConverter]::ToString($sha256.ComputeHash([IO.File]::ReadAllBytes($header))).Replace('-', '').ToLowerInvariant()
} finally { $sha256.Dispose() }
if ($headerHash -ne $pin.nlohmann.sha256) { throw 'Vendored nlohmann header hash mismatch.' }
& $cmake -S $repo -B $BuildDirectory -G $generator -A x64 "-DCMAKE_GENERATOR_INSTANCE=$vs"
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& $cmake --build $BuildDirectory --config Release --parallel 4
if ($LASTEXITCODE) { throw 'Windows build failed.' }
& $ctest --test-dir $BuildDirectory -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Contract tests failed.' }
$output = Join-Path $PSScriptRoot 'out/windows-x86_64'
New-Item -ItemType Directory -Force -Path (Join-Path $output 'notices') | Out-Null
Copy-Item -LiteralPath (Join-Path $BuildDirectory 'Release/companion.exe'),(Join-Path $BuildDirectory 'Release/window-animations-preview.exe') -Destination $output
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE'),(Join-Path $repo 'THIRD_PARTY_NOTICES.md') -Destination $output
Copy-Item -LiteralPath (Join-Path $repo 'third_party/nlohmann/LICENSE.MIT') -Destination (Join-Path $output 'notices/nlohmann-MIT.txt')
Write-Host "Window Animations bundle: $output"
