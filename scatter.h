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

#ifndef SCATTER_INCLUDED
#define SCATTER_INCLUDED

// What the Scatter and Calibration charts draw, independent of the GUI so
// it can be tested. Values are in guide-camera pixels.

#include "analysis.h"
#include "graphmath.h"
#include "logparser.h"

#include <optional>
#include <vector>

struct PlotPoint
{
    double x, y;
};

// RA against Dec (or dx against dy) for the frames that count toward the
// statistics, within `range` if given.
std::vector<PlotPoint> ScatterPoints(const GuideSession& session, const std::optional<IndexRun>& range, bool raDecAxes);

// The farthest any point lies from the origin.
double MaxRadius(const std::vector<PlotPoint>& points);

// The ellipse that shows the shape of the scatter: centred on the mean,
// along the axes of elongation, two standard deviations each way.
struct Ellipse
{
    double cx, cy;   // centre
    double rx, ry;   // semi-axes
    double angle;    // of rx from the x-axis, radians
};
Ellipse ElongationEllipse(const GuideStats& stats);

// Each calibration step's offset from where the calibration started.
std::vector<PlotPoint> CalibrationPoints(const Calibration& cal);

// The first and last steps of a direction, for the line that shows its
// path, or nothing when it took fewer than two steps.
std::optional<IndexRun> DirectionSpan(const Calibration& cal, CalDirection direction);

#endif
