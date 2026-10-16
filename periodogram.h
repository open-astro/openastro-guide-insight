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

#ifndef PERIODOGRAM_INCLUDED
#define PERIODOGRAM_INCLUDED

// What the Frequency chart draws and says, independent of the GUI so it
// can be tested.

#include "analysis.h"
#include "labels.h"

#include <optional>
#include <string>
#include <vector>

// Ticks for a logarithmic axis from p0 to p1: 1, 2 and 5 times powers of ten.
std::vector<double> LogTicks(double p0, double p1);

// The index of the local maximum nearest to i, no more than `reach` away,
// or i itself when there is none. Used to snap the pointer to a peak.
size_t SnapToPeak(const std::vector<double>& values, size_t i, size_t reach);

// The index of the periodogram's largest amplitude, if it has any points.
std::optional<size_t> LargestPeak(const GARun& ga);

// The status line for a point on the periodogram:
// "period 480 s · amplitude 1.20″ · peak to peak 2.40″ · RMS 0.85″".
std::string PeriodText(double period, double amplitude, double pixelScale, Units units);

#endif
