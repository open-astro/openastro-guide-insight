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

#ifndef TIMELINE_INCLUDED
#define TIMELINE_INCLUDED

// What the Timeline chart draws, independent of the GUI so it can be
// tested. Values are in guide-camera pixels, positive up.

#include "graphmath.h"
#include "logparser.h"
#include "viewport.h"

#include <optional>
#include <vector>

enum class Series { Ra, Dec };

// A frame's offset on a series: RA or Dec, or the camera's dx or dy.
double SeriesValue(const GuideEntry& e, Series series, bool raDecAxes);

// A frame's guide pulse as the distance it moves the star, signed the same
// way as the series. Zero for pulses sent to the other device.
double CorrectionValue(const GuideSession& session, const GuideEntry& e, Series series, WhichMount device);

// A guide algorithm's limits on one axis, as distances: its minimum move,
// and how far its longest allowed pulse moves the star. Zero when the log
// does not give one.
struct AxisLimits
{
    double minMove;
    double maxPulse;
};
AxisLimits LimitsFor(const GuideSession& session, Series series, WhichMount device);

// Runs of excluded frames among entries [i0, i1].
std::vector<IndexRun> ExcludedRuns(const GuideSession::EntryVec& entries, unsigned int i0, unsigned int i1);

// The frames to draw for a view from position `first` to `last`: those in
// view plus one either side, so lines run to the edges. Nothing when no
// frame is in or next to the view.
std::optional<IndexRun> FramesToDraw(const GuideSession::EntryVec& entries, double first, double last, XAxis axis);

// The frame nearest an x-axis position, or nothing for an empty session.
std::optional<size_t> FrameAt(const GuideSession::EntryVec& entries, double position, XAxis axis);

// Rows for labels that start at left[i] and are width[i] wide, so that
// labels in the same row keep `gap` apart: 0 is the first row.
std::vector<int> StackLabels(const std::vector<double>& left, const std::vector<double>& width, double gap);

// Whether the session has guide pulses for both a mount and an AO.
bool HasBothDevices(const GuideSession& session);

#endif
