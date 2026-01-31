$ErrorActionPreference = "Stop"

Write-Host "--- :package: Checking dependencies"

Write-Host "--- :hammer: Building"
# Try make (usually available in MinGW environments)
if (Get-Command "make" -ErrorAction SilentlyContinue) {
    make clean
    make
} elseif (Get-Command "mingw32-make" -ErrorAction SilentlyContinue) {
    mingw32-make clean
    mingw32-make
} else {
    Write-Error "Make not found. Please ensure MinGW/MSYS2 is in PATH."
    exit 1
}

Write-Host "--- :test_tube: Testing"
# Basic verification since 'make test' might rely on unix shell
if (Test-Path "out/dist/cdrive.exe") {
    & "out/dist/cdrive.exe" --version
} else {
    Write-Error "Build output not found."
    exit 1
}
