param(
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Push-Location $PSScriptRoot
try {
    if ($Clean -and (Test-Path "build")) {
        Remove-Item -Recurse -Force "build"
    }

    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

        cmake --build build
        if ($LASTEXITCODE -ne 0) { throw "Build failed." }

        $exe = Join-Path $PSScriptRoot "build\PencilBridge.exe"
    }
    else {
        cmake -S . -B build -A x64
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

        cmake --build build --config Release
        if ($LASTEXITCODE -ne 0) { throw "Build failed." }

        $exe = Join-Path $PSScriptRoot "build\Release\PencilBridge.exe"
    }

    if (!(Test-Path $exe)) {
        throw "Build completed but PencilBridge.exe was not found at $exe"
    }

    Write-Host ""
    Write-Host "Built: $exe"
}
finally {
    Pop-Location
}
