# vcpkg overlay ports

`wxwidgets/` pins wxWidgets **3.2.12**, the current release of the 3.2
stable series. phdlogview stays on 3.2 until wxWidgets 3.4 ships.

The port is vcpkg's `wxwidgets` port for 3.3.3, whose patches also apply to
3.2.12. It drops the two 3.3-only patches (libsecret link directories and
the Wayland build fix), does not download the lexilla and scintilla
submodules because 3.2 bundles Scintilla, and uses 3.2 names for the CMake package directory,
`wxrc`, the include directory and the Windows library infix. Remove this overlay once
vcpkg's registry carries the wxWidgets version we want.

To move to another 3.2.x release:

1. Set `version` in `wxwidgets/vcpkg.json`.
2. Update `SHA512` in `wxwidgets/portfile.cmake` to the SHA-512 of
   `https://github.com/wxWidgets/wxWidgets/archive/v<version>.tar.gz`.
3. Update `PHDLOGVIEW_EXPECTED_WX_VERSION` in `CMakePresets.json` and in
   the Linux CI job.
