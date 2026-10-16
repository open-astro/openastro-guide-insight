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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <math.h>

using Catch::Approx;

namespace {

GuideSession Session()
{
    GuideSession s("2026-01-01 00:00:00");
    for (int i = 0; i < 6; i++)
    {
        GuideEntry e{};
        e.included = true;
        e.raraw = (float) i;
        e.decraw = (float) -i;
        e.dx = 10.f + i;
        e.dy = 20.f + i;
        s.entries.push_back(e);
    }
    s.entries[2].included = false;
    s.entries[4].err = 2; // star lost
    return s;
}

} // namespace

TEST_CASE("The scatter shows the frames that count", "[scatter]")
{
    auto p = ScatterPoints(Session(), std::nullopt, true);
    REQUIRE(p.size() == 4);
    CHECK(p[2].x == 3.);
    CHECK(p[2].y == -3.);
}

TEST_CASE("The scatter can show the camera axes", "[scatter]")
{
    auto p = ScatterPoints(Session(), std::nullopt, false);
    REQUIRE(p.size() == 4);
    CHECK(p[0].x == 10.);
    CHECK(p[0].y == 20.);
}

TEST_CASE("The scatter can show just the selection", "[scatter]")
{
    auto p = ScatterPoints(Session(), IndexRun{ 1, 3 }, true);
    REQUIRE(p.size() == 2);
    CHECK(p[0].x == 1.);
    CHECK(p[1].x == 3.);
}

TEST_CASE("The farthest point sets the radius", "[scatter]")
{
    CHECK(MaxRadius({ { 3., 4. }, { -1., 0. } }) == Approx(5.));
    CHECK(MaxRadius({}) == 0.);
}

TEST_CASE("The elongation ellipse spans two standard deviations", "[scatter]")
{
    GuideStats st;
    st.avg_ra = 0.5;
    st.avg_dec = -0.25;
    st.lx = 1.;
    st.ly = 0.5;
    st.theta = 0.3;
    Ellipse e = ElongationEllipse(st);
    CHECK(e.cx == 0.5);
    CHECK(e.cy == -0.25);
    CHECK(e.rx == Approx(2.));
    CHECK(e.ry == Approx(1.));
    CHECK(e.angle == Approx(0.3));
}

TEST_CASE("Calibration steps are offsets from the start", "[scatter]")
{
    Calibration cal("2026-01-01 00:00:00");
    cal.entries = { { WEST, 0, 0.f, 0.f }, { WEST, 1, 1.f, 2.f }, { WEST, 2, 2.f, 4.f },
                    { NORTH, 0, 2.f, 4.f }, { NORTH, 1, 3.f, 1.f } };
    auto p = CalibrationPoints(cal);
    REQUIRE(p.size() == 5);
    CHECK(p[1].x == 1.);
    CHECK(p[1].y == 2.);

    auto west = DirectionSpan(cal, WEST);
    REQUIRE(west);
    CHECK(west->first == 0);
    CHECK(west->last == 2);
    auto north = DirectionSpan(cal, NORTH);
    REQUIRE(north);
    CHECK(north->first == 3);
    CHECK(north->last == 4);
    CHECK_FALSE(DirectionSpan(cal, EAST));
}
