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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using Catch::Approx;

namespace {

std::vector<std::string> Labels(const std::vector<TimeTick>& ticks)
{
    std::vector<std::string> v;
    for (const auto& t : ticks)
        v.push_back(t.label);
    return v;
}

wxDateTime At(const char *iso)
{
    wxDateTime t;
    t.ParseISOCombined(iso, ' ');
    return t;
}

} // namespace

TEST_CASE("Nice steps are 1, 2 or 5 times a power of ten", "[ticks]")
{
    CHECK(NiceStep(0.7) == Approx(1.0));
    CHECK(NiceStep(1.0) == Approx(1.0));
    CHECK(NiceStep(1.3) == Approx(2.0));
    CHECK(NiceStep(2.0) == Approx(2.0));
    CHECK(NiceStep(3.1) == Approx(5.0));
    CHECK(NiceStep(6.0) == Approx(10.0));
    CHECK(NiceStep(0.04) == Approx(0.05));
    CHECK(NiceStep(180.0) == Approx(200.0));
}

TEST_CASE("A nice step of nothing is no step", "[ticks]")
{
    CHECK(NiceStep(0.0) == 0.0);
    CHECK(NiceStep(-1.0) == 0.0);
}

TEST_CASE("Steps between two values are the multiples of the step", "[ticks]")
{
    CHECK(StepsBetween(-2.5, 3.0, 1.0) == std::vector<double>{ -2.0, -1.0, 0.0, 1.0, 2.0, 3.0 });
    CHECK(StepsBetween(0.1, 0.9, 1.0).empty());
}

TEST_CASE("Steps land on the ends despite rounding", "[ticks]")
{
    auto v = StepsBetween(0.0, 0.3, 0.1);
    REQUIRE(v.size() == 4);
    CHECK(v.back() == Approx(0.3));
}

TEST_CASE("No step gives no steps", "[ticks]")
{
    CHECK(StepsBetween(0.0, 10.0, 0.0).empty());
}

TEST_CASE("Scale rings are nice multiples out to the largest radius", "[ticks]")
{
    // 6.3 / 4 = 1.6 rounds up to a 2" step
    CHECK(RingRadii(6.3, 4) == std::vector<double>{ 2.0, 4.0, 6.0 });
    CHECK(RingRadii(1.0, 4) == std::vector<double>{ 0.5, 1.0 });
}

TEST_CASE("No scatter, no scale rings", "[ticks]")
{
    CHECK(RingRadii(0.0, 4).empty());
}

TEST_CASE("Time steps are clock-friendly", "[ticks]")
{
    CHECK(TimeStep(0.5) == 1.0);
    CHECK(TimeStep(3.0) == 5.0);
    CHECK(TimeStep(12.0) == 15.0);
    CHECK(TimeStep(45.0) == 60.0);
    CHECK(TimeStep(61.0) == 120.0);
    CHECK(TimeStep(400.0) == 600.0);
    CHECK(TimeStep(1000.0) == 1800.0);
    CHECK(TimeStep(5000.0) == 7200.0);
    CHECK(TimeStep(20000.0) == 21600.0);
    CHECK(TimeStep(50000.0) == 86400.0);
}

TEST_CASE("Very long time spans step by whole days", "[ticks]")
{
    CHECK(TimeStep(200000.0) == 3.0 * 86400.0);
}

TEST_CASE("Time ticks fall on whole minutes of the clock", "[ticks]")
{
    // the session starts 13 s before 07:07
    auto ticks = TimeTicks(At("2026-10-04 07:06:47"), 0.0, 600.0, 60.0);
    REQUIRE(ticks.size() == 10);
    CHECK(ticks.front().t == Approx(13.0));
    CHECK(ticks.front().label == "07:07");
    CHECK(ticks.back().t == Approx(553.0));
    CHECK(ticks.back().label == "07:16");
}

TEST_CASE("Ten-minute ticks fall on the tens", "[ticks]")
{
    auto ticks = TimeTicks(At("2026-10-04 07:06:47"), 0.0, 1800.0, 600.0);
    CHECK(Labels(ticks) == std::vector<std::string>{ "07:10", "07:20", "07:30" });
}

TEST_CASE("Ticks under a minute apart show seconds", "[ticks]")
{
    auto ticks = TimeTicks(At("2026-10-04 07:06:47"), 0.0, 40.0, 15.0);
    CHECK(Labels(ticks) == std::vector<std::string>{ "07:07:00", "07:07:15" });
}

TEST_CASE("Ticks start from the visible part of the session", "[ticks]")
{
    auto ticks = TimeTicks(At("2026-10-04 07:06:47"), 100.0, 250.0, 60.0);
    CHECK(Labels(ticks) == std::vector<std::string>{ "07:09", "07:10" });
}

TEST_CASE("Ticks carry on past midnight", "[ticks]")
{
    auto ticks = TimeTicks(At("2026-10-04 23:50:30"), 0.0, 1200.0, 600.0);
    CHECK(Labels(ticks) == std::vector<std::string>{ "00:00", "00:10" });
}

TEST_CASE("Labels that would overlap the one before are skipped", "[ticks]")
{
    // across a pause in guiding, ticks bunch up on the frame axis
    std::vector<double> left{ 0.0, 10.0, 30.0, 60.0, 62.0 };
    std::vector<double> width(5, 25.0);
    CHECK(LabelsThatFit(left, width, 4.0) == std::vector<bool>{ true, false, true, true, false });
}
