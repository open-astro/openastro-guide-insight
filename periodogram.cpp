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

#include "periodogram.h"

#include <algorithm>
#include <math.h>

// Room for rounding error when an end falls on a tick.
static const double EPS = 1e-9;

std::vector<double> LogTicks(double p0, double p1)
{
    std::vector<double> ticks;
    if (!(p0 > 0.) || !(p1 > p0))
        return ticks;

    int d0 = (int) floor(log10(p0)), d1 = (int) ceil(log10(p1));
    for (int d = d0; d <= d1; d++)
        for (double m : { 1., 2., 5. })
        {
            double v = m * pow(10., d);
            if (v >= p0 * (1. - EPS) && v <= p1 * (1. + EPS))
                ticks.push_back(v);
        }
    return ticks;
}

size_t SnapToPeak(const std::vector<double>& values, size_t i, size_t reach)
{
    auto isPeak = [&values](size_t k) {
        return k > 0 && k + 1 < values.size() && values[k] > values[k - 1] && values[k] > values[k + 1];
    };

    // look outwards from i, left before right, so a tie goes left
    for (size_t d = 0; d <= reach; d++)
    {
        if (d <= i && isPeak(i - d))
            return i - d;
        if (isPeak(i + d))
            return i + d;
    }
    return i;
}

std::optional<size_t> LargestPeak(const GARun& ga)
{
    if (ga.nfft == 0)
        return std::nullopt;
    return (size_t) (std::max_element(ga.ffty, ga.ffty + ga.nfft) - ga.ffty);
}

std::string PeriodText(double period, double amplitude, double pixelScale, Units units)
{
    const char *dot = " · ";
    return "period " + Fixed(period, period < 100. ? 1 : 0) + " s" +
        dot + "amplitude " + Distance(amplitude, pixelScale, units) +
        dot + "peak to peak " + Distance(2. * amplitude, pixelScale, units) +
        dot + "RMS " + Distance(amplitude * M_SQRT1_2, pixelScale, units);
}
