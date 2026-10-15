/*
 *  This file is part of phdlogview
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

#ifndef GRAPHMATH_INCLUDED
#define GRAPHMATH_INCLUDED

// Graph layout calculations, independent of the GUI so they can be tested.

#include "logparser.h"

#include <vector>

// An inclusive range of entry indices.
struct IndexRun
{
    unsigned int first;
    unsigned int last;
};

// Runs of consecutive entries in [i0, i1] where the star was found. A line
// graph draws one line per run, leaving gaps where the star was lost.
std::vector<IndexRun> FoundRuns(const GuideSession::EntryVec& entries, unsigned int i0, unsigned int i1);

// Position of time t (seconds from the start of the session) as a fractional
// entry index, interpolating between the frames either side of it. Frames
// are not evenly spaced in time, so this is what places time-axis labels.
// Times before the first frame or after the last clamp to the ends.
double IndexAtTime(const GuideSession::EntryVec& entries, double t);

#endif
