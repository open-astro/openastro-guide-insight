# Contributing to OpenAstro Guide Insight

Thanks for helping. This page covers how changes are made, how the code is laid out, and how to build, test and package it.

## How we work: test first, in small steps

We follow Kent Beck's Extreme Programming practice of test-driven development:

1. **Red.** Write a test that shows the bug or describes the new behaviour, and run it to watch it fail.
2. **Green.** Make the simplest change that passes it.
3. **Refactor.** Tidy the code with every test passing. A refactor never changes behaviour.

Keep each step small. Integrate often: one issue per pull request, with CI green on every platform before it merges. Keep the design simple, and add only what a test or a user needs.

GUI code is hard to test, so keep it thin. Put calculations, parsing and the text the window shows in the core library, with tests, and leave drawing and events in the window classes.

## Layout

- **Core library** (`guideinsight_core`, no GUI, all tested):
  - `logparser`: reads PHD2 guide logs.
  - `analysis`: statistics and the Guiding Assistant analysis.
  - `exclusion`: the exclusion rules.
  - `document`: the open log, the current section and selection, and each session's view.
  - `viewport` and `ticks`: chart geometry.
  - `timeline`, `scatter` and `periodogram`: what each chart draws.
  - `labels` and `sessiondetails`: the text the window shows.
  - `appsettings` and `settingsmigration`: saved settings.
- **Window** (wxWidgets):
  - `MainFrame`: the window, toolbar and menus.
  - `SessionSidebar` and `Inspector`: the two side panels.
  - `ChartView`: the middle of the window.
  - `TimelineCanvas`, `ScatterCanvas`, `DriftCanvas`, `FrequencyCanvas` and `CalibrationCanvas`: one per chart.
  - `OverviewScrubber`: the strip under the Timeline.
  - `SegmentedControl` and `SectionHeader`: small custom controls.
- **Tests** are in `tests/`, using [Catch2](https://github.com/catchorg/Catch2). `tests/data` holds a real guide log.
- **Packaging** scripts are in `scripts/`: `make-dmg.sh`, `make-appimage.sh`, the Inno Setup script `openastro-guide-insight.iss.in`, and a `check-*` script for each package.

The layout is plain C++; there is no form designer.

## Building and testing

See [Building from source](README.md#building-from-source) for each platform. Then run the tests with `ctest --preset <preset>`.

- Run one area of the tests by tag: `out/build/<preset>/tests/guideinsight_tests "[timeline]"`.
- On macOS and Linux, `cmake --preset asan` builds with AddressSanitizer and UndefinedBehaviorSanitizer. Warnings are errors there, as they are in CI.

## The version

The version is written once, in `project(guideinsight VERSION x.y.z)` in `CMakeLists.txt`. CMake carries it into the About box, the macOS bundle, the installer and the package names. A test checks that they agree.

## Continuous integration

CI builds and tests:

- Windows x64 and ARM64
- macOS on Apple Silicon
- Linux x86_64 and aarch64
- an ASan/UBSan build on Linux

It then builds each platform's package and checks it:

- **macOS:** the disk image mounts and holds an arm64-only, signed app.
- **Windows:** the installer installs and uninstalls cleanly.
- **Linux:** the AppImage starts.

Watch for APIs that build on one platform and not another. For example, the Linux wxWidgets build has no SVG support, and the macOS build machines' C++ library has no `std::from_chars` for floating point.

## Pull requests

Fill in the checklist in the pull request template. Describe what changed and why, and link the issue (`Closes #n`).
