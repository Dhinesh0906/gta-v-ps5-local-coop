Loader 043 and original 040 snapshots include their C/assembly/linker sources
and frozen generated headers containing the compiled custom helper images.
build-loader.sh links the loader from those frozen headers, without a GTA dump.
For example: PS5_SDK=/path/to/sdk/install bash build-loader.sh
A built PS5 Payload SDK and LLVM toolchain are prerequisites; see its upstream
build instructions. Compilation is not required to install the supplied ELFs.
Helper sources are retained for modification; their fixed layouts/linker scripts
must be respected when regenerating images. Do not change only the byte arrays.

PS5Debug-NG upstream commit: fc9aef98f3fb9b908d90b81e2a879c3b79b11f89
The copied source includes the local main.c change that starts its server
without waiting for an Ethernet/Wi-Fi address. The patch is also supplied.
See ps5debug-NG/README.md and build.sh for its SDK and build requirements.
Third-party license notices and vendored source files are retained.

License: GPL-3.0 for the main project and debugger; individual third-party
notices apply where indicated. Retain sources and licenses when redistributing.
No game executable, game archive, BIOS, save, key, or console database included.
