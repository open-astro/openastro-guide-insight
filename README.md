# OpenAstro Guide Insight

[![CI](https://github.com/open-astro/openastro-guide-insight/actions/workflows/ci.yml/badge.svg)](https://github.com/open-astro/openastro-guide-insight/actions/workflows/ci.yml)

See and analyze your [PHD2](https://openphdguiding.org) guide logs on Windows, macOS and Linux.

Open a `PHD2_GuideLog_*.txt` file to:

- Browse every calibration and guiding session in the log.
- Plot RA/Dec error, guide pulses, star mass and SNR over time, with dithers, settling and other events marked.
- Get RMS, peak, elongation, drift and polar alignment error for a session or any part of it.
- Exclude settling, pauses or any range you choose from the statistics.
- Analyze unguided runs, such as the Guiding Assistant, for drift and periodic error, and export the periodogram as CSV.

## Platforms

| Platform | Architectures |
|---|---|
| Windows 10/11 | x64, ARM64 |
| macOS 26 or later | Apple Silicon |
| Linux | x86_64, aarch64 |

## Building from source

You need CMake 3.24+, Ninja, a C++20 compiler and [vcpkg](https://vcpkg.io).

**macOS (Apple Silicon)**

```
export VCPKG_ROOT=~/vcpkg
cmake --preset macos-arm64
cmake --build --preset macos-arm64
ctest --preset macos-arm64
```

**Windows** (from a Developer Command Prompt)

```
set VCPKG_ROOT=C:\vcpkg
cmake --preset win-x64        :: or win-arm64
cmake --build --preset win-x64
ctest --preset win-x64
```

**Linux**

Install GTK 3, GSL and the other build dependencies, build the pinned wxWidgets, then configure against it:

```
sudo apt install ninja-build libgtk-3-dev libgsl-dev libpcre2-dev zlib1g-dev libpng-dev libjpeg-dev libtiff-dev libexpat1-dev
scripts/build-wxwidgets.sh ~/wx
cmake --preset linux -DCMAKE_PREFIX_PATH=$HOME/wx
cmake --build --preset linux
ctest --preset linux
```

The app uses wxWidgets **3.2.12**, pinned on every platform until wxWidgets 3.4. See [`vcpkg-ports/README.md`](vcpkg-ports/README.md).

## Credits

OpenAstro Guide Insight is a fork of [PHD2 Log Viewer](https://github.com/agalasso/phdlogview) by Andy Galasso, which is no longer maintained. Thank you, Andy, for the original app.

## License

GNU General Public License, version 3 or later. See [LICENSE.txt](LICENSE.txt). Copyright (C) 2016-2020 Andy Galasso, (C) 2026 OpenAstro contributors.
