# PowerShell compile script for MinGW-w64 (Windows)
# Run from project root:  powershell -ExecutionPolicy Bypass -File .\build\compile_gcc_mingw.ps1

$src = Get-ChildItem -Recurse -Filter *.c -Path .\src | ForEach-Object { $_.FullName }

# Output
$out = ".\build\showroom.exe"

# Raylib + ODBC libs (adjust if your raylib installation differs)
$libs = "-lraylib -lglfw3 -lopengl32 -lgdi32 -lwinmm -lodbc32"

# Include directory
$inc = "-Iinclude"

# Compile
& gcc @src $inc -o $out $libs

if ($LASTEXITCODE -eq 0) {
  Write-Host "Build success: $out"
} else {
  Write-Host "Build failed. Check errors above."
}
