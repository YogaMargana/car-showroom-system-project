# Project Showroom Mobil (restructured)

Perubahan yang saya lakukan dari zip awal:

1) Semua header dipindahkan dari folder `Header/` ke `include/`.
2) Semua source `.c` dipindahkan ke `src/` dan dikelompokkan per domain:
   - `src/core` : core app (auth, koneksi DB, menu role)
   - `src/ui` : komponen UI (ui, textbox)
   - `src/db` : repository/DB access (file `db_*.c`)
   - `src/pages` : halaman UI (landing/login) + subfolder per role (`admin/`, `cashier/`, `sales/`)
3) Asset gambar dipindahkan ke `assets/` dan pemanggilan `LoadTexture(...)` di `src/main.c` sudah disesuaikan.
4) `#include "Header/..."` dan `#include "header/..."` sudah disederhanakan menjadi `#include "..."` sehingga cukup memakai `-Iinclude`.
5) Tambah skrip build:
   - Windows MinGW: `build/compile_gcc_mingw.ps1`
   - Linux: `build/compile_linux.sh`

## Struktur folder

- `include/` : semua `.h`
- `src/` : semua `.c`
- `assets/` : png/logo
- `build/` : output binary + script compile
- `.vscode/` : konfigurasi IntelliSense

## Compile (Windows, MinGW-w64)

Jalankan dari root project:

```powershell
powershell -ExecutionPolicy Bypass -File .\build\compile_gcc_mingw.ps1
```

Atau manual:

```powershell
$src = Get-ChildItem -Recurse -Filter *.c -Path .\src | % { $_.FullName }
gcc @src -Iinclude -o .\build\showroom.exe -lraylib -lglfw3 -lopengl32 -lgdi32 -lwinmm -lodbc32
```

## Catatan runtime asset

Karena asset berada di `assets/`, jalankan executable dengan working directory = folder project (root) agar `assets/...` terbaca.
