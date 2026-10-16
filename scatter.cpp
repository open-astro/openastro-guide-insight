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

#include "scatter.h"

#include <algorithm>
#include <math.h>

std::vector<PlotPoint> ScatterPoints(const GuideSession& session, const std::optional<IndexRun>& range, bool raDecAxes)
{
    const auto& entries = session.entries;
    size_t first = range ? range->first : 0;
    size_t last = range ? std::min<size_t>(range->last, entries.size() - 1) : entries.size() - 1;

    std::vector<PlotPoint> points;
    for (size_t i = first; i <= last && i < entries.size(); i++)
    {
        const GuideEntry& e = entries[i];
        if (!e.included || !StarWasFound(e.err))
            continue;
        points.push_back(raDecAxes ? PlotPoint{ e.raraw, e.decraw } : PlotPoint{ e.dx, e.dy });
    }
    return points;
}

double MaxRadius(const std::vector<PlotPoint>& points)
{
    double r = 0.;
    for (const PlotPoint& p : points)
        r = std::max(r, hypot(p.x, p.y));
    return r;
}

Ellipse ElongationEllipse(const GuideStats& stats)
{
    return Ellipse{ stats.avg_ra, stats.avg_dec, 2. * stats.lx, 2. * stats.ly, stats.theta };
}

std::vector<PlotPoint> CalibrationPoints(const Calibration& cal)
{
    std::vector<PlotPoint> points;
    for (const auto& e : cal.entries)
        points.push_back(PlotPoint{ e.dx, e.dy });
    return points;
}

std::optional<IndexRun> DirectionSpan(const Calibration& cal, CalDirection direction)
{
    std::optional<IndexRun> span;
    for (unsigned int i = 0; i < cal.entries.size(); i++)
    {
        if (cal.entries[i].direction != direction)
            continue;
        if (span)
            span->last = i;
        else
            span = IndexRun{ i, i };
    }
    if (span && span->first == span->last)
        return std::nullopt;
    return span;
}
