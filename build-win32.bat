if not exist .\vcpkg\ (
   	git clone https://github.com/microsoft/vcpkg
   	.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
)
if not exist .\vcpkg\installed\x86-windows-static\lib\zlib.lib (
	.\vcpkg\vcpkg install zlib:x86-windows-static
)
if not exist .\vcpkg\installed\x86-windows-static\lib\libpng16.lib (
	.\vcpkg\vcpkg install libpng:x86-windows-static
)
rmdir build-win32 /s /q
meson setup --buildtype=release build-win32
meson compile -C build-win32

