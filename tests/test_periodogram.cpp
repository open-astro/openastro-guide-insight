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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <math.h>

using Catch::Approx;

TEST_CASE("Log ticks are 1, 2 and 5 in each decade", "[periodogram]")
{
    auto t = LogTicks(3., 250.);
    std::vector<double> want{ 5., 10., 20., 50., 100., 200. };
    REQUIRE(t.size() == want.size());
    for (size_t i = 0; i < t.size(); i++)
        CHECK(t[i] == Approx(want[i]));
}

TEST_CASE("Log ticks include the ends when they fall on a tick", "[periodogram]")
{
    auto t = LogTicks(10., 100.);
    REQUIRE(t.size() == 4);
    CHECK(t.front() == Approx(10.));
    CHECK(t.back() == Approx(100.));
}

TEST_CASE("No range, no log ticks", "[periodogram]")
{
    CHECK(LogTicks(0., 10.).empty());
    CHECK(LogTicks(10., 5.).empty());
}

TEST_CASE("The pointer snaps to the nearest peak", "[periodogram]")
{
    //                         0   1   2   3   4   5   6   7   8
    std::vector<double> v{ 1., 2., 5., 3., 2., 2.5, 6., 4., 1. };
    CHECK(SnapToPeak(v, 3, 3) == 2);
    CHECK(SnapToPeak(v, 5, 3) == 6);
    CHECK(SnapToPeak(v, 4, 3) == 2); // a tie goes to the left
    CHECK(SnapToPeak(v, 2, 3) == 2);
}

TEST_CASE("With no peak in reach the pointer stays put", "[periodogram]")
{
    std::vector<double> rising{ 1., 2., 3., 4., 5., 6., 7. };
    CHECK(SnapToPeak(rising, 3, 2) == 3);
    CHECK(SnapToPeak(rising, 3, 10) == 3); // the ends are not peaks
}

TEST_CASE("The largest peak is found in a sine wave's periodogram", "[periodogram]")
{
    GuideSession s("2026-01-01 00:00:00");
    for (int i = 0; i < 300; i++)
    {
        GuideEntry e{};
        e.included = true;
        e.dt = 2.f * (float) i;
        e.raraw = (float) sin(2. * M_PI * e.dt / 120.);
        s.entries.push_back(e);
    }
    GARun ga;
    ga.Analyze(s, 0, s.entries.size(), false);
    auto peak = LargestPeak(ga);
    REQUIRE(peak);
    CHECK(ga.fftx[*peak] == Approx(120.).epsilon(0.05));
}

TEST_CASE("An empty periodogram has no peak", "[periodogram]")
{
    GARun ga;
    CHECK_FALSE(LargestPeak(ga));
}

TEST_CASE("The readout gives the period and the amplitude in the chosen units", "[periodogram]")
{
    CHECK(PeriodText(480.4, 0.2, 6., Units::ArcSeconds) ==
          "period 480 s · amplitude 1.20″ · peak to peak 2.40″ · RMS 0.85″");
    CHECK(PeriodText(12.34, 0.2, 6., Units::Pixels) ==
          "period 12.3 s · amplitude 0.20 px · peak to peak 0.40 px · RMS 0.14 px");
}
