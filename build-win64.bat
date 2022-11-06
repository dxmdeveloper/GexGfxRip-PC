if not exist .\vcpkg\ (
   git clone https://github.com/microsoft/vcpkg
   .\vcpkg\bootstrap-vcpkg.bat -disableMetrics
)
if not exist .\vcpkg\installed\x64-windows-static\lib\zlib.lib (
	.\vcpkg\vcpkg install zlib:x64-windows-static
)
if not exist .\vcpkg\installed\x64-windows-static\lib\libpng16.lib (
	.\vcpkg\vcpkg install libpng:x64-windows-static
)
rmdir build-win64 /s /q
meson setup --buildtype=release build-win64
meson compile -C build-win64

