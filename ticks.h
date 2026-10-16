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

#ifndef TICKS_INCLUDED
#define TICKS_INCLUDED

// Where to put grid lines, axis labels and scale rings, independent of the
// GUI so they can be tested.

#include <wx/datetime.h>

#include <string>
#include <vector>

// The smallest "nice" step, 1, 2 or 5 times a power of ten, that is at
// least `approx`.
double NiceStep(double approx);

// The multiples of `step` from lo to hi inclusive, in increasing order.
std::vector<double> StepsBetween(double lo, double hi, double step);

// Radii for the scatter plot's scale rings: multiples of a nice step, about
// `count` of them, out to maxRadius.
std::vector<double> RingRadii(double maxRadius, int count);

// The smallest time-axis step, in seconds, from a list of clock-friendly
// steps (1 s, 2 s, 5 s ... 1 min, 2 min ... 1 h ... 12 h, 1 day), that is at
// least minSeconds.
double TimeStep(double minSeconds);

struct TimeTick
{
    double t;          // seconds from the start of the session
    std::string label; // local clock time, "07:10" or "07:10:30"
};

// Ticks between t0 and t1 (seconds from `start`) at local clock times that
// are multiples of `step` seconds, so a 10-minute step ticks at 07:10,
// 07:20 and so on. Labels show seconds only when the step is under a minute.
std::vector<TimeTick> TimeTicks(const wxDateTime& start, double t0, double t1, double step);

// Which labels to draw, left to right, so that none overlaps the previous
// one drawn: label i starts at left[i], is width[i] wide, and needs `gap`
// of space after the label before it. The first label is always drawn.
std::vector<bool> LabelsThatFit(const std::vector<double>& left, const std::vector<double>& width, double gap);

#endif
