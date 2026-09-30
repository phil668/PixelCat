#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p dist/PixelCat-Windows
x86_64-w64-mingw32-g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -municode -mwindows src/windows.cpp -lgdiplus -lshell32 -lgdi32 -luser32 -ladvapi32 -o dist/PixelCat-Windows/PixelCat.exe
cp assets/cat.png assets/actions.png assets/groom.png assets/flower.png assets/belly.png assets/downcast.png assets/stretch.png assets/doze.png dist/PixelCat-Windows/
echo "Built dist/PixelCat-Windows/PixelCat.exe"
