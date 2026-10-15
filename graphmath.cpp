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

#include "graphmath.h"

#include <algorithm>

std::vector<IndexRun> FoundRuns(const GuideSession::EntryVec& entries, unsigned int i0, unsigned int i1)
{
    std::vector<IndexRun> runs;
    bool inRun = false;

    for (unsigned int i = i0; i <= i1 && i < entries.size(); i++)
    {
        if (StarWasFound(entries[i].err))
        {
            if (inRun)
                runs.back().last = i;
            else
                runs.push_back(IndexRun{ i, i });
            inRun = true;
        }
        else
            inRun = false;
    }

    return runs;
}

double IndexAtTime(const GuideSession::EntryVec& entries, double t)
{
    if (entries.size() < 2 || t <= entries.front().dt)
        return 0.;
    if (t >= entries.back().dt)
        return (double)(entries.size() - 1);

    // first entry later than t; the one before it is at or before t
    auto hi = std::upper_bound(entries.begin(), entries.end(), t,
                               [](double v, const GuideEntry& e) { return v < e.dt; });
    auto lo = hi - 1;

    double span = hi->dt - lo->dt;
    double frac = span > 0. ? (t - lo->dt) / span : 0.;
    return (double)(lo - entries.begin()) + frac;
}
