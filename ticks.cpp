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

#include "ticks.h"

#include <math.h>

// Room for rounding error when a value should land exactly on a step.
static const double EPS = 1e-9;

double NiceStep(double approx)
{
    if (!(approx > 0.))
        return 0.;

    double mag = pow(10., floor(log10(approx)));
    for (double m : { 1., 2., 5., 10. })
        if (m * mag >= approx * (1. - EPS))
            return m * mag;
    return 10. * mag;
}

std::vector<double> StepsBetween(double lo, double hi, double step)
{
    std::vector<double> v;
    if (!(step > 0.))
        return v;

    long long k0 = (long long) ceil(lo / step - EPS);
    long long k1 = (long long) floor(hi / step + EPS);
    for (long long k = k0; k <= k1; k++)
        v.push_back((double) k * step);
    return v;
}

std::vector<double> RingRadii(double maxRadius, int count)
{
    if (!(maxRadius > 0.) || count < 1)
        return {};

    double step = NiceStep(maxRadius / count);
    return StepsBetween(step, maxRadius, step);
}

double TimeStep(double minSeconds)
{
    static const double steps[] = {
        1, 2, 5, 10, 15, 30,
        60, 2 * 60, 5 * 60, 10 * 60, 15 * 60, 30 * 60,
        3600, 2 * 3600, 3 * 3600, 6 * 3600, 12 * 3600, 86400,
    };
    for (double s : steps)
        if (s >= minSeconds)
            return s;
    return ceil(minSeconds / 86400.) * 86400.;
}

std::vector<TimeTick> TimeTicks(const wxDateTime& start, double t0, double t1, double step)
{
    std::vector<TimeTick> ticks;
    if (!(step > 0.) || !start.IsValid())
        return ticks;

    // seconds after local midnight at the start of the session; every step
    // divides a day, so multiples of it land on the same clock times each day
    double sod = start.GetHour() * 3600. + start.GetMinute() * 60. + start.GetSecond() +
        start.GetMillisecond() / 1000.;

    const char *format = step < 60. ? "%H:%M:%S" : "%H:%M";
    for (double clock : StepsBetween(sod + t0, sod + t1, step))
    {
        double t = clock - sod;
        wxDateTime when = start + wxTimeSpan::Milliseconds(llround(t * 1000.));
        ticks.push_back(TimeTick{ t, when.Format(format).ToStdString() });
    }
    return ticks;
}

std::vector<bool> LabelsThatFit(const std::vector<double>& left, const std::vector<double>& width, double gap)
{
    std::vector<bool> fits;
    double end = -INFINITY; // right edge of the last label drawn, plus the gap
    for (size_t i = 0; i < left.size(); i++)
    {
        bool fit = left[i] >= end;
        fits.push_back(fit);
        if (fit)
            end = left[i] + (i < width.size() ? width[i] : 0.) + gap;
    }
    return fits;
}
