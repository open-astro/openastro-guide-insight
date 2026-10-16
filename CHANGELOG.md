# Changelog

All notable changes to OpenAstro Guide Insight, and to PHD2 Log Viewer before it.

## Unreleased

- Renamed from PHD2 Log Viewer; settings carry over
- New window: a sidebar of sessions, the chart, and an inspector of statistics, session details, display, analysis and exclusion options
- The chart switches between Timeline, Scatter, Drift and Frequency; the separate analysis window is gone
- Timeline: drag to select frames and see their statistics; scroll, pinch and an overview strip to move around; a time axis option; hover for any frame's details
- Scatter: full size, with labelled scale rings
- Drift: the drift taken out can be a straight line, a curve, or none
- Frequency: pointing snaps to the nearest peak; export as CSV from the File menu
- Light and dark appearance, standard menus and shortcuts, Open Recent, Preferences, and a rewritten Help
- Exclusion settings now apply to every session when changed
- Downloads: a Windows installer for x64 and ARM64, a macOS disk image for Apple Silicon (macOS 26 or later), and Linux AppImages for x86_64 and aarch64
- `--version` prints the version
- Fixed: RA plotted against its grid labels' sign, guide limits that ignored the AO, a failed open clearing the current log, file names with apostrophes not opening in the text editor

## 0.6.5 - 24 May 2026

- compute elongation statistic and display ellipse in scatter plot
- exclude star-lost frames from scatter plot and stats

## 0.6.4 - 12 Jan 2020

- Fix problem with log section grid scrollbars sometimes missing
- Fix location of right-click popup menu

## 0.6.3 - 15 Dec 2019

- High DPI screen fixes - allow resizing panels and auto-size grids to fit content
- handle logs with backwards timestamp jumps

## 0.6.2 - 13 March 2018

- Analysis view: add an uption to "undo" RA corrections to view unguided RA tracking
- When vertical scale is locked, lock the arc-seconds scale, not the pixel scale
- Windows: add missing runtime DLLs

## 0.6.1 - 13 February 2018

- Analysis view: fix problem with saturated star frames being incorrectly excluded from the analysis
- Analysis view: make grid lines more visible and add a y-axis scale to the periodogram
- Analysis view: add read-out of the cursor position for both the drift-corrected view and the periodogram

## 0.6.0 - 8 February 2018

- Display RA and Dec drift information
- Analyze Guiding Assistant runs to show drift-corrected data and a frequency analysis view
- Label guide pulse directions on the vertical axis
- Add a button to open the log file in a text editor
- Add an option to toggle display between AO and Mount corrections

## 0.5.1 - 11 July 2017

- Change license to GPLv3
- Recognize Min Move setting for Predictive PEC guide algo
- Fix incorrectly displayed Frame Dropped label for saturated star frames

## 0.5.0 - 8 December 2016

- Added a button to toggle between pan / zoom for the vertical axis (or use P or Z on the keyboard)
- Added a button to lock the vertical scale (use the same vertical scale for all guide sessions)
- Added options to make the RA/Dec colors configurable and added a legend for the RA/Dec (or dx/dy) colors
- Values from the stats grid can be copied to the clipboard
- Allow the Settle Pixels setting to have 2 decimal digits
- Vertical orientation is now the same as PHD2's graph
- Show the PHD2 version from the log in the window's title bar
- Do not leave stale row info displayed when switching sections
- Show Saturation in the row info line
- Linux: remember the last file open location

## 0.4.3 - 21 February 2015

- Allow running on Windows XP
- scale scatter plot zoom level along with graph

## 0.4.2 - 14 February 2015

- add timestamps to horizontal axis

## 0.4.1 - 12 February 2015

- added a scatter plot option

## 0.4.1 - 11 February 2015

- more include/exclude options, better panning, resize display for calibration

## 0.3.2 - 7 February 2015

- fix hang when resizing the window to a very small size
- rescale mass & snr curves when window is resized; better reset scale behavior
- File open dialog filters-out files that are not guide logs
- added a help screen
- control-drag to exclude ranges
