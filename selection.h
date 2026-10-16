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

#ifndef SELECTION_INCLUDED
#define SELECTION_INCLUDED

// A range of frames picked on the chart, independent of the GUI so it can
// be tested.

#include "graphmath.h"
#include "logparser.h"
#include "viewport.h"

#include <optional>

// The frames whose x-axis positions lie between pos0 and pos1, in either
// order, or nothing when no frame does.
std::optional<IndexRun> FramesBetween(const GuideSession::EntryVec& entries, double pos0, double pos1, XAxis axis);

// Includes or excludes every frame of the run.
void IncludeFrames(GuideSession& session, const IndexRun& run, bool include);

#endif
