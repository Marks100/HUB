@echo off
:: build.bat — compile seedkey.cpp as a 64-bit DLL using MinGW-w64
::
:: Prerequisites: MinGW-w64 on PATH  (e.g. MSYS2: pacman -S mingw-w64-x86_64-gcc)
::
:: Output: seedkey.dll  (64-bit, compatible with 64-bit Python and VFlash)

x86_64-w64-mingw32-g++ ^
    -shared ^
    -std=c++11 ^
    -O2 ^
    -o seedkey.dll ^
    seedkey.cpp ^
    -Wl,--kill-at

if %ERRORLEVEL% == 0 (
    echo.
    echo Build successful: seedkey.dll
) else (
    echo.
    echo Build FAILED — is MinGW-w64 on your PATH?
)
