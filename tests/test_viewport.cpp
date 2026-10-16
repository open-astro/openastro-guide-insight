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

#include "viewport.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>

using Catch::Approx;

namespace {

// 1000 frames on a 500-pixel chart, at most 100 pixels per frame.
Viewport Frames1000()
{
    Viewport v(100.);
    v.SetContent(0., 1000.);
    v.SetWidth(500.);
    return v;
}

GuideSession::EntryVec Entries(std::initializer_list<float> times)
{
    GuideSession::EntryVec v;
    for (float t : times)
    {
        GuideEntry e{};
        e.dt = t;
        v.push_back(e);
    }
    return v;
}

} // namespace

TEST_CASE("A new view shows the whole session", "[viewport]")
{
    Viewport v = Frames1000();
    CHECK(v.First() == Approx(0.));
    CHECK(v.Last() == Approx(1000.));
    CHECK(v.Scale() == Approx(0.5));
}

TEST_CASE("Positions map to x and back", "[viewport]")
{
    Viewport v = Frames1000();
    CHECK(v.XAt(0.) == Approx(0.));
    CHECK(v.XAt(500.) == Approx(250.));
    CHECK(v.PositionAt(250.) == Approx(500.));
}

TEST_CASE("Zooming keeps the point under the pointer in place", "[viewport]")
{
    Viewport v = Frames1000();
    double before = v.PositionAt(100.);
    v.ZoomAbout(4., 100.);
    CHECK(v.Scale() == Approx(2.));
    CHECK(v.PositionAt(100.) == Approx(before));
    CHECK(v.Last() - v.First() == Approx(250.));
}

TEST_CASE("Zooming out stops at the whole session", "[viewport]")
{
    Viewport v = Frames1000();
    CHECK_FALSE(v.CanZoomOut());
    v.ZoomAbout(0.5, 250.);
    CHECK(v.Scale() == Approx(0.5));
    CHECK(v.First() == Approx(0.));
}

TEST_CASE("Zooming in stops at the closest zoom", "[viewport]")
{
    Viewport v = Frames1000();
    CHECK(v.CanZoomIn());
    v.ZoomAbout(1000., 250.);
    CHECK(v.Scale() == Approx(100.));
    CHECK_FALSE(v.CanZoomIn());
    CHECK(v.CanZoomOut());
}

TEST_CASE("Zooming out near an end keeps the view inside the session", "[viewport]")
{
    Viewport v = Frames1000();
    v.Show(900., 1000.);
    v.ZoomAbout(0.5, 0.);
    CHECK(v.Last() == Approx(1000.));
    CHECK(v.First() == Approx(800.));
}

TEST_CASE("Scrolling moves the view by pixels", "[viewport]")
{
    Viewport v = Frames1000();
    v.Show(100., 200.); // 5 px per frame
    v.Scroll(50.);
    CHECK(v.First() == Approx(110.));
    CHECK(v.Last() == Approx(210.));
}

TEST_CASE("Scrolling stops at the ends of the session", "[viewport]")
{
    Viewport v = Frames1000();
    v.Show(100., 200.);
    v.Scroll(-10000.);
    CHECK(v.First() == Approx(0.));
    v.Scroll(100000.);
    CHECK(v.Last() == Approx(1000.));
    CHECK(v.First() == Approx(900.));
}

TEST_CASE("Showing a range outside the session slides it inside", "[viewport]")
{
    Viewport v = Frames1000();
    v.Show(-50., 50.);
    CHECK(v.First() == Approx(0.));
    CHECK(v.Last() == Approx(100.));
}

TEST_CASE("Showing a range given backwards shows the same range", "[viewport]")
{
    Viewport v = Frames1000();
    v.Show(300., 200.);
    CHECK(v.First() == Approx(200.));
    CHECK(v.Last() == Approx(300.));
}

TEST_CASE("Resizing keeps the visible range", "[viewport]")
{
    Viewport v = Frames1000();
    v.Show(100., 200.);
    v.SetWidth(1000.);
    CHECK(v.First() == Approx(100.));
    CHECK(v.Last() == Approx(200.));
    CHECK(v.Scale() == Approx(10.));
}

TEST_CASE("A session too short to fill the width is drawn at the closest zoom", "[viewport]")
{
    Viewport v(100.);
    v.SetContent(0., 3.);
    v.SetWidth(500.);
    CHECK(v.Scale() == Approx(100.));
    CHECK(v.First() == Approx(0.));
    CHECK_FALSE(v.CanZoomIn());
    CHECK_FALSE(v.CanZoomOut());
    v.Scroll(50.);
    CHECK(v.First() == Approx(0.));
}

TEST_CASE("A view with no width yet fits once it has one", "[viewport]")
{
    Viewport v(100.);
    v.SetContent(0., 1000.);
    v.SetWidth(0.);
    v.SetWidth(500.);
    CHECK(v.First() == Approx(0.));
    CHECK(v.Last() == Approx(1000.));
}

TEST_CASE("A view of a session that starts later counts from its start", "[viewport]")
{
    Viewport v(100.);
    v.SetContent(8., 608.);
    v.SetWidth(600.);
    CHECK(v.First() == Approx(8.));
    CHECK(v.XAt(8.) == Approx(0.));
    CHECK(v.XAt(608.) == Approx(600.));
}

TEST_CASE("Fitting the vertical view leaves a margin", "[viewport]")
{
    VerticalView v;
    v.Fit(4., 300.); // 140 px for 4 px of offset
    CHECK(v.Scale() == Approx(35.));
    CHECK(v.YAt(0., 300.) == Approx(150.));
    CHECK(v.YAt(4., 300.) == Approx(10.));
    CHECK(v.YAt(-4., 300.) == Approx(290.));
}

TEST_CASE("A flat session fits at one pixel per pixel", "[viewport]")
{
    VerticalView v;
    v.Fit(0., 300.);
    CHECK(v.Scale() == Approx(1.));
}

TEST_CASE("Panning the vertical view moves the zero line", "[viewport]")
{
    VerticalView v;
    v.Fit(4., 300.);
    v.Pan(20.);
    CHECK(v.YAt(0., 300.) == Approx(170.));
    CHECK(v.ValueAt(170., 300.) == Approx(0.));
    v.Fit(4., 300.);
    CHECK(v.YAt(0., 300.) == Approx(150.));
}

TEST_CASE("Zooming the vertical view scales about zero", "[viewport]")
{
    VerticalView v;
    v.Fit(4., 300.);
    v.Zoom(2.);
    CHECK(v.YAt(1., 300.) == Approx(80.));
    CHECK(v.ValueAt(80., 300.) == Approx(1.));
}

TEST_CASE("The frame axis counts frames", "[viewport]")
{
    auto e = Entries({ 8.f, 10.f, 14.f });
    CHECK(AxisPosition(e, 2, XAxis::Frames) == 2.);
    CHECK(IndexAtPosition(e, 1.5, XAxis::Frames) == 1.5);
    double start, end;
    AxisSpan(e, XAxis::Frames, &start, &end);
    CHECK(start == 0.);
    CHECK(end == 2.);
}

TEST_CASE("The time axis counts seconds", "[viewport]")
{
    auto e = Entries({ 8.f, 10.f, 14.f });
    CHECK(AxisPosition(e, 2, XAxis::Time) == 14.);
    CHECK(IndexAtPosition(e, 12., XAxis::Time) == Approx(1.5));
    double start, end;
    AxisSpan(e, XAxis::Time, &start, &end);
    CHECK(start == 8.);
    CHECK(end == 14.);
}

TEST_CASE("An empty session spans nothing", "[viewport]")
{
    GuideSession::EntryVec e;
    double start = -1., end = -1.;
    AxisSpan(e, XAxis::Time, &start, &end);
    CHECK(start == 0.);
    CHECK(end == 0.);
}
