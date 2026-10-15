# vcpkg triplet for macOS Apple Silicon release builds.
#
# Same as the built-in arm64-osx triplet, but pins the deployment target so
# the static dependencies run on the same macOS versions as phdlogview
# itself. Without this, vcpkg targets the build machine's macOS version.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET 11.0)
