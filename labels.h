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

// The statistics as CSV, for the clipboard.
std::string StatsCsv(const GuideStats& stats, double pixelScale);

#endif
