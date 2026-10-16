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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

using Catch::Approx;

namespace {

GuideEntry Entry(float t, bool included = true)
{
    GuideEntry e{};
    e.dt = t;
    e.included = included;
    e.mount = MOUNT;
    e.raraw = 1.5f;
    e.decraw = -0.5f;
    e.dx = 0.25f;
    e.dy = 0.75f;
    return e;
}

// Frames 2 s apart from 8 s; 'x' frames are excluded.
GuideSession::EntryVec Entries(const std::string& pattern)
{
    GuideSession::EntryVec v;
    for (size_t i = 0; i < pattern.size(); i++)
        v.push_back(Entry(8.f + 2.f * (float) i, pattern[i] != 'x'));
    return v;
}

GuideSession MountAndAo()
{
    GuideSession s("2026-01-01 00:00:00");
    s.mount.isValid = true;
    s.mount.xRate = 2.0;  // px per second
    s.mount.yRate = 4.0;
    s.mount.xlim.minMo = 0.15;
    s.mount.xlim.maxDur = 2500.;
    s.mount.ylim.minMo = 0.2;
    s.mount.ylim.maxDur = 1000.;
    s.ao.isValid = true;
    s.ao.xRate = 10.0;
    s.ao.yRate = 10.0;
    s.ao.xlim.minMo = 0.05;
    s.ao.xlim.maxDur = 0.;
    return s;
}

} // namespace

TEST_CASE("Series show RA and Dec, or the camera axes", "[timeline]")
{
    GuideEntry e = Entry(0.f);
    CHECK(SeriesValue(e, Series::Ra, true) == 1.5);
    CHECK(SeriesValue(e, Series::Dec, true) == -0.5);
    CHECK(SeriesValue(e, Series::Ra, false) == 0.25);
    CHECK(SeriesValue(e, Series::Dec, false) == 0.75);
}

TEST_CASE("A guide pulse is the distance it moves the star", "[timeline]")
{
    GuideSession s = MountAndAo();
    GuideEntry e = Entry(0.f);
    e.radur = 500;   // ms west
    e.decdur = -250; // ms south
    CHECK(CorrectionValue(s, e, Series::Ra, MOUNT) == Approx(1.0));
    CHECK(CorrectionValue(s, e, Series::Dec, MOUNT) == Approx(-1.0));
}

TEST_CASE("Pulses sent to the other device are not shown", "[timeline]")
{
    GuideSession s = MountAndAo();
    GuideEntry e = Entry(0.f);
    e.mount = AO;
    e.radur = 100; // AO steps
    CHECK(CorrectionValue(s, e, Series::Ra, MOUNT) == 0.);
    CHECK(CorrectionValue(s, e, Series::Ra, AO) == Approx(1.0));
}

TEST_CASE("Limits are the minimum move and the longest pulse", "[timeline]")
{
    GuideSession s = MountAndAo();
    AxisLimits ra = LimitsFor(s, Series::Ra, MOUNT);
    CHECK(ra.minMove == Approx(0.15));
    CHECK(ra.maxPulse == Approx(5.0)); // 2500 ms at 2 px/s
    AxisLimits dec = LimitsFor(s, Series::Dec, MOUNT);
    CHECK(dec.minMove == Approx(0.2));
    CHECK(dec.maxPulse == Approx(4.0));
}

TEST_CASE("Limits follow the chosen device", "[timeline]")
{
    // the old graph always drew the mount's limits
    AxisLimits ao = LimitsFor(MountAndAo(), Series::Ra, AO);
    CHECK(ao.minMove == Approx(0.05));
    CHECK(ao.maxPulse == 0.);
}

TEST_CASE("Excluded runs are found within the range", "[timeline]")
{
    auto e = Entries("ooxxoxooxx");
    auto runs = ExcludedRuns(e, 0, 9);
    REQUIRE(runs.size() == 3);
    CHECK(runs[0].first == 2);
    CHECK(runs[0].last == 3);
    CHECK(runs[1].first == 5);
    CHECK(runs[1].last == 5);
    CHECK(runs[2].first == 8);
    CHECK(runs[2].last == 9);

    auto part = ExcludedRuns(e, 3, 6);
    REQUIRE(part.size() == 2);
    CHECK(part[0].first == 3);
    CHECK(part[1].last == 5);
}

TEST_CASE("The frames to draw reach one past each edge", "[timeline]")
{
    auto e = Entries("oooooooooo");
    auto f = FramesToDraw(e, 2.5, 6.5, XAxis::Frames);
    REQUIRE(f);
    CHECK(f->first == 2);
    CHECK(f->last == 7);

    auto t = FramesToDraw(e, 11., 15., XAxis::Time); // frames at 8, 10 ... 26 s
    REQUIRE(t);
    CHECK(t->first == 1);
    CHECK(t->last == 4);
}

TEST_CASE("The frames to draw stop at the ends of the session", "[timeline]")
{
    auto e = Entries("ooooo");
    auto f = FramesToDraw(e, -10., 100., XAxis::Frames);
    REQUIRE(f);
    CHECK(f->first == 0);
    CHECK(f->last == 4);
    CHECK_FALSE(FramesToDraw(GuideSession::EntryVec(), 0., 10., XAxis::Frames));
}

TEST_CASE("The frame under the pointer is the nearest one", "[timeline]")
{
    auto e = Entries("ooooo"); // 8, 10, 12, 14, 16 s
    CHECK(FrameAt(e, 2.4, XAxis::Frames) == 2u);
    CHECK(FrameAt(e, 2.6, XAxis::Frames) == 3u);
    CHECK(FrameAt(e, -3., XAxis::Frames) == 0u);
    CHECK(FrameAt(e, 99., XAxis::Frames) == 4u);
    CHECK(FrameAt(e, 12.9, XAxis::Time) == 2u);
    CHECK(FrameAt(e, 13.1, XAxis::Time) == 3u);
    CHECK_FALSE(FrameAt(GuideSession::EntryVec(), 0., XAxis::Frames));
}

TEST_CASE("Labels that would overlap go to the next row with room", "[timeline]")
{
    std::vector<double> left{ 0., 30., 45., 50., 200., 210. };
    std::vector<double> width{ 40., 40., 40., 40., 20., 20. };
    CHECK(StackLabels(left, width, 10.) == std::vector<int>{ 0, 1, 2, 0, 0, 1 });
}

TEST_CASE("A label that fits goes back to the first row", "[timeline]")
{
    std::vector<double> left{ 0., 20., 100. };
    std::vector<double> width{ 40., 40., 20. };
    CHECK(StackLabels(left, width, 10.) == std::vector<int>{ 0, 1, 0 });
}

TEST_CASE("A session guiding both a mount and an AO has both devices", "[timeline]")
{
    GuideSession s = MountAndAo();
    s.entries = Entries("oooo");
    CHECK_FALSE(HasBothDevices(s));
    s.entries[2].mount = AO;
    CHECK(HasBothDevices(s));
}
