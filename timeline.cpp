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

#include "timeline.h"

#include <algorithm>
#include <math.h>

double SeriesValue(const GuideEntry& e, Series series, bool raDecAxes)
{
    if (raDecAxes)
        return series == Series::Ra ? e.raraw : e.decraw;
    return series == Series::Ra ? e.dx : e.dy;
}

double CorrectionValue(const GuideSession& session, const GuideEntry& e, Series series, WhichMount device)
{
    if (e.mount != device)
        return 0.;
    const Mount& m = e.mount == AO ? session.ao : session.mount;
    // durations are milliseconds and rates pixels per second
    return series == Series::Ra ? e.radur * m.xRate / 1000. : e.decdur * m.yRate / 1000.;
}

AxisLimits LimitsFor(const GuideSession& session, Series series, WhichMount device)
{
    const Mount& m = device == AO ? session.ao : session.mount;
    const Limits& l = series == Series::Ra ? m.xlim : m.ylim;
    double rate = series == Series::Ra ? m.xRate : m.yRate;
    return AxisLimits{ l.minMo, l.maxDur * rate / 1000. };
}

std::vector<IndexRun> ExcludedRuns(const GuideSession::EntryVec& entries, unsigned int i0, unsigned int i1)
{
    std::vector<IndexRun> runs;
    bool inRun = false;
    for (unsigned int i = i0; i <= i1 && i < entries.size(); i++)
    {
        if (!entries[i].included)
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

std::optional<IndexRun> FramesToDraw(const GuideSession::EntryVec& entries, double first, double last, XAxis axis)
{
    if (entries.empty())
        return std::nullopt;

    long n = (long) entries.size();
    long a, b;
    if (axis == XAxis::Time)
    {
        // the last frame at or before `first`, and the first at or after `last`
        auto lo = std::upper_bound(entries.begin(), entries.end(), first,
                                   [](double t, const GuideEntry& e) { return t < e.dt; });
        auto hi = std::lower_bound(entries.begin(), entries.end(), last,
                                   [](const GuideEntry& e, double t) { return e.dt < t; });
        a = (long) (lo - entries.begin()) - 1;
        b = (long) (hi - entries.begin());
    }
    else
    {
        a = (long) floor(first);
        b = (long) ceil(last);
    }

    a = std::max(a, 0L);
    b = std::min(b, n - 1);
    if (a > b)
        return std::nullopt;
    return IndexRun{ (unsigned int) a, (unsigned int) b };
}

std::optional<size_t> FrameAt(const GuideSession::EntryVec& entries, double position, XAxis axis)
{
    if (entries.empty())
        return std::nullopt;
    double index = IndexAtPosition(entries, position, axis);
    double last = (double) (entries.size() - 1);
    return (size_t) std::clamp(floor(index + 0.5), 0., last);
}

std::vector<int> StackLabels(const std::vector<double>& left, const std::vector<double>& width, double gap)
{
    std::vector<int> rows;
    std::vector<double> ends; // where each row's last label ends
    for (size_t i = 0; i < left.size(); i++)
    {
        size_t row = 0;
        while (row < ends.size() && left[i] < ends[row] + gap)
            row++;
        if (row == ends.size())
            ends.push_back(0.);
        ends[row] = left[i] + (i < width.size() ? width[i] : 0.);
        rows.push_back((int) row);
    }
    return rows;
}

bool HasBothDevices(const GuideSession& session)
{
    bool mount = false, ao = false;
    for (const auto& e : session.entries)
    {
        mount |= e.mount == MOUNT;
        ao |= e.mount == AO;
    }
    return mount && ao;
}
