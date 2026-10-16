/*
 *  This file is part of OpenAstro Guide Insight, a fork of PHD2 Log Viewer
 *
 *  Copyright (C) 2026 Open Astro contributors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, visit the http://fsf.org website.
 */

#ifndef LABELS_INCLUDED
#define LABELS_INCLUDED

// The text the window shows for numbers, times and sections, independent
// of the GUI so it can be tested. Numbers always use a decimal point, and
// negative numbers a typographic minus sign. Strings are UTF-8.

#include "analysis.h"
#include "logparser.h"

#include <wx/datetime.h>

#include <string>
#include <vector>

enum class Units { ArcSeconds, Pixels };

// v with `decimals` digits after the point: "1.51", "−0.40".
std::string Fixed(double v, int decimals);

// The same with a sign either way: "+0.12", "−0.40".
std::string Signed(double v, int decimals);

// A length of time: "45 s", "2 min 40 s", "1 h 5 min".
std::string Duration(double seconds);

// A distance given in guide-camera pixels, in the chosen units:
// "1.51″" or "0.23 px".
std::string Distance(double pixels, double pixelScale, Units units, int decimals = 2);

// One row of the session list.
struct SectionRow
{
    bool calibration;
    std::string kind; // "Guiding", "Calibration" ...
    std::string meta; // "07:06 · 2 min 40 s" or "06:58 · 110 steps"
    std::string rms;  // total RMS, empty for a calibration
};
SectionRow RowFor(const GuideLog& log, size_t section, Units units);

// A section's heading over the chart: "Guiding · 07:06:47".
std::string SectionTitle(const GuideLog& log, size_t section);

// The heading for several sessions shown as one:
// "3 sessions · 07:06 – 08:40", from the first's start to the last's end.
std::string CombinedTitle(const GuideLog& log, const std::vector<size_t>& sections);

// The day a log starts: "Sunday, October 4, 2026".
std::string DayHeading(const wxDateTime& when);

// How long the log spent guiding, over all its sessions.
double TotalGuiding(const GuideLog& log);

// The inspector's Summary and Drift sections.
struct SummaryText
{
    std::string total;      // total RMS in the chosen units
    std::string alt;        // the same in the other units
    std::string ra, dec;    // RMS
    std::string peak;       // "RA 1.60″ · Dec −6.32″"
    std::string elongation; // "18%"
    std::string frames;     // "149 of 160 frames"
    std::string raDrift, decDrift; // "+0.12″/min"
    std::string paError;    // "3.2′"
};
SummaryText Summarize(const GuideStats& stats, size_t frames, double pixelScale, Units units);

// The status line for the frame under the pointer:
// "07:14:45 · frame 53 · RA −0.03″ · Dec 0.12″ · pulse RA 120 ms W · SNR 52.1 · mass 30411".
// It also notes a saturated star, a lost star, an excluded frame and the
// frame's event, if any.
std::string HoverText(const GuideSession& session, size_t frame, Units units, bool raDecAxes);

// The chart's subtitle for a view of a session from t0 to t1 seconds:
// "Whole session" or "07:18 – 07:30 of 27 min".
std::string VisibleRange(const GuideSession& session, double t0, double t1);

// The statistics as CSV, for the clipboard.
std::string StatsCsv(const GuideStats& stats, double pixelScale);

#endif
