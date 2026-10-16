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

#include "selection.h"

#include "exclusion.h"

#include <algorithm>
#include <math.h>

std::optional<IndexRun> FramesBetween(const GuideSession::EntryVec& entries, double pos0, double pos1, XAxis axis)
{
    if (entries.empty())
        return std::nullopt;

    double lo = std::min(pos0, pos1);
    double hi = std::max(pos0, pos1);
    double n = (double) entries.size();

    double first, last;
    if (axis == XAxis::Time)
    {
        // the first frame at or after lo, and the last at or before hi
        auto a = std::lower_bound(entries.begin(), entries.end(), lo,
                                  [](const GuideEntry& e, double t) { return e.dt < t; });
        auto b = std::upper_bound(entries.begin(), entries.end(), hi,
                                  [](double t, const GuideEntry& e) { return t < e.dt; });
        first = (double) (a - entries.begin());
        last = (double) (b - entries.begin()) - 1.;
    }
    else
    {
        first = std::max(ceil(lo), 0.);
        last = std::min(floor(hi), n - 1.);
    }

    if (first > last)
        return std::nullopt;
    return IndexRun{ (unsigned int) first, (unsigned int) last };
}

void IncludeFrames(GuideSession& session, const IndexRun& run, bool include)
{
    IncludeRange(session.entries, include, run.first, run.last + 1);
}
