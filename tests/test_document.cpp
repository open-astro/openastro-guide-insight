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

#include "document.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

using Catch::Approx;

namespace {

const char *const REAL_LOG = TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt";

// The 2026-10-04 log: a calibration, then two guiding sessions.
Document RealLog()
{
    Document doc;
    std::ifstream ifs(REAL_LOG);
    REQUIRE(doc.Load(ifs));
    REQUIRE(doc.Log().sections.size() == 3);
    return doc;
}

size_t Excluded(const GuideSession& s)
{
    size_t n = 0;
    for (const auto& e : s.entries)
        if (!e.included)
            n++;
    return n;
}

// A log with one session of n frames 2 s apart and pixel scale `scale`.
std::string OneSession(int n, double scale, double offset)
{
    std::ostringstream os;
    os << "Guiding Begins at 2026-10-04 07:00:00\n"
       << "Pixel scale = " << scale << " arc-sec/px, Binning = 1, Focal length = 120 mm\n"
       << "Frame,Time,mount,dx,dy,RARawDistance,DECRawDistance,RAGuideDistance,DECGuideDistance,RADuration,RADirection,DECDuration,DECDirection,XStep,YStep,StarMass,SNR,ErrorCode\n";
    for (int i = 1; i <= n; i++)
        os << i << "," << 2 * i << ",\"Mount\",0,0," << offset << "," << -offset
           << ",0,0,0,,0,,,,1000,20,0\n";
    os << "Guiding Ends at 2026-10-04 08:00:00\n\n";
    return os.str();
}

Document Load(const std::string& text)
{
    Document doc;
    std::istringstream is(text);
    REQUIRE(doc.Load(is));
    return doc;
}

} // namespace

TEST_CASE("Opening a log makes its first section current", "[document]")
{
    Document doc = RealLog();
    REQUIRE(doc.Current());
    CHECK(*doc.Current() == 0);
    CHECK(doc.CalibrationSection());
    CHECK_FALSE(doc.Session());
}

TEST_CASE("Opening a log applies the exclusion settings and the statistics", "[document]")
{
    Document doc = RealLog();
    doc.Select(1);
    REQUIRE(doc.Session());
    CHECK(Excluded(*doc.Session()) == 11); // settling at the start
    CHECK(doc.Session()->rms_ra > 0.);
}

TEST_CASE("A log that cannot be read leaves the open log alone", "[document]")
{
    Document doc = RealLog();
    std::ifstream missing(TEST_DATA_DIR "/no-such-log.txt");
    CHECK_FALSE(doc.Load(missing));
    CHECK(doc.Log().sections.size() == 3);
    CHECK(doc.Current());
}

TEST_CASE("Opening a long log reports progress", "[document]")
{
    Document doc;
    std::ifstream ifs(REAL_LOG);
    int calls = 0;
    REQUIRE(doc.Load(ifs, [&calls] { calls++; }));
    CHECK(calls > 0);
}

TEST_CASE("An empty log has no current section", "[document]")
{
    Document doc = Load("");
    CHECK_FALSE(doc.Current());
    CHECK_FALSE(doc.Session());
    CHECK_FALSE(doc.View());
}

TEST_CASE("Changing the exclusion settings applies them to every session", "[document]")
{
    Document doc = RealLog();
    ExclusionSettings none;
    none.settlingByServer = false;
    doc.SetExclusions(none);
    for (const auto& s : doc.Log().sessions)
        CHECK(Excluded(s) == 0);

    doc.SetExclusions(ExclusionSettings());
    for (const auto& s : doc.Log().sessions)
        CHECK(Excluded(s) > 0);
}

TEST_CASE("Changing the exclusion settings updates the statistics", "[document]")
{
    Document doc = RealLog();
    doc.Select(1);
    double settled = doc.Session()->peak_dec;
    ExclusionSettings none;
    none.settlingByServer = false;
    doc.SetExclusions(none);
    CHECK(doc.Session()->peak_dec != settled);
}

TEST_CASE("Choosing another section clears the selection", "[document]")
{
    Document doc = RealLog();
    doc.Select(1);
    doc.SetSelection(IndexRun{ 20, 40 });
    doc.Select(2);
    CHECK_FALSE(doc.Selection());
}

TEST_CASE("Statistics cover the selection, or the whole session", "[document]")
{
    Document doc = RealLog();
    doc.Select(2);
    GuideSession *s = doc.Session();
    CHECK(doc.Stats().rms_ra == s->rms_ra);

    doc.SetSelection(IndexRun{ 50, 99 });
    GuideStats part = CalcStats(*s, 50, 100);
    CHECK(doc.Stats().frames == part.frames);
    CHECK(doc.Stats().rms_ra == part.rms_ra);
}

TEST_CASE("Excluding the selection excludes its frames and updates the statistics", "[document]")
{
    Document doc = RealLog();
    doc.Select(2);
    GuideSession *s = doc.Session();
    size_t before = Excluded(*s);
    double rms = s->rms_ra;

    doc.SetSelection(IndexRun{ 50, 99 });
    doc.IncludeSelection(false);
    CHECK(Excluded(*s) >= before + 40);
    CHECK(s->rms_ra != rms);
    CHECK(doc.Stats().frames == 0);
}

TEST_CASE("Including with no selection includes the whole session", "[document]")
{
    Document doc = RealLog();
    doc.Select(2);
    doc.IncludeSelection(true);
    CHECK(Excluded(*doc.Session()) == 0);
    doc.IncludeSelection(false);
    CHECK(Excluded(*doc.Session()) == doc.Session()->entries.size());
}

TEST_CASE("Including or excluding all frames ignores the selection", "[document]")
{
    Document doc = RealLog();
    doc.Select(2);
    doc.SetSelection(IndexRun{ 50, 99 });
    doc.IncludeAll(false);
    CHECK(Excluded(*doc.Session()) == doc.Session()->entries.size());
    CHECK(doc.Selection());
    doc.IncludeAll(true);
    CHECK(Excluded(*doc.Session()) == 0);
    CHECK(doc.Session()->rms_ra > 0.);
}

TEST_CASE("Frames can be excluded and included without selecting them", "[document]")
{
    Document doc = RealLog();
    doc.Select(2);
    doc.IncludeAll(true);
    doc.SetSelection(IndexRun{ 0, 9 });
    doc.IncludeFrames(IndexRun{ 50, 59 }, false);
    CHECK(Excluded(*doc.Session()) == 10);
    CHECK(doc.Selection()->first == 0); // the selection stays
    doc.IncludeFrames(IndexRun{ 55, 70 }, true);
    CHECK(Excluded(*doc.Session()) == 5);
}

TEST_CASE("A new session view has not been fitted vertically", "[document]")
{
    Document doc = Load(OneSession(10, 2., 4.));
    CHECK_FALSE(doc.View()->verticalFitted);
    doc.FitVertical(300.);
    CHECK(doc.View()->verticalFitted);
}

TEST_CASE("A calibration has no selection, statistics or view", "[document]")
{
    Document doc = RealLog();
    doc.SetSelection(IndexRun{ 0, 5 });
    CHECK_FALSE(doc.Selection());
    CHECK(doc.Stats().frames == 0);
    CHECK_FALSE(doc.View());
    doc.IncludeSelection(false); // nothing to do, and no crash
    doc.IncludeAll(false);
}

TEST_CASE("Each session keeps its own view", "[document]")
{
    Document doc = RealLog();
    doc.Select(1);
    doc.View()->horizontal.SetWidth(500.);
    doc.View()->horizontal.Show(10., 20.);
    doc.Select(2);
    doc.View()->horizontal.SetWidth(500.);
    CHECK(doc.View()->horizontal.First() == Approx(0.));
    doc.Select(1);
    CHECK(doc.View()->horizontal.First() == Approx(10.));
}

TEST_CASE("A new view fits the whole session", "[document]")
{
    Document doc = Load(OneSession(101, 2., 1.));
    doc.View()->horizontal.SetWidth(500.);
    CHECK(doc.View()->horizontal.First() == Approx(0.));
    CHECK(doc.View()->horizontal.Last() == Approx(100.));
}

TEST_CASE("Switching to a time axis keeps the same frames in view", "[document]")
{
    Document doc = Load(OneSession(101, 2., 1.)); // frames at 2, 4 ... 202 s
    Viewport& h = doc.View()->horizontal;
    h.SetWidth(500.);
    h.Show(10., 20.);
    doc.SetXAxis(XAxis::Time);
    CHECK(doc.GetXAxis() == XAxis::Time);
    CHECK(doc.View()->horizontal.First() == Approx(22.));
    CHECK(doc.View()->horizontal.Last() == Approx(42.));
    doc.SetXAxis(XAxis::Frames);
    CHECK(doc.View()->horizontal.First() == Approx(10.));
    CHECK(doc.View()->horizontal.Last() == Approx(20.));
}

TEST_CASE("A time axis zooms in as far as a frame axis", "[document]")
{
    Document doc = Load(OneSession(101, 2., 1.)); // 2 s per frame
    doc.SetXAxis(XAxis::Time);
    Viewport& h = doc.View()->horizontal;
    h.SetWidth(500.);
    h.ZoomAbout(1e6, 0.);
    CHECK(h.Scale() == Approx(50.)); // 100 px per frame
}

TEST_CASE("Fitting the vertical view uses the largest offset", "[document]")
{
    Document doc = Load(OneSession(10, 2., 4.));
    CHECK(MaxOffset(*doc.Session()) == Approx(4.));
    doc.FitVertical(300.);
    CHECK(doc.View()->vertical.Scale() == Approx(35.));
}

TEST_CASE("Fitting the vertical view leaves out excluded frames", "[document]")
{
    // settling after a dither can be far larger than the guiding itself
    Document doc = RealLog();
    doc.Select(1);
    const GuideSession& s = *doc.Session();
    double included = 0.;
    for (const auto& e : s.entries)
        if (e.included)
            included = std::max({ included, (double) fabs(e.raraw), (double) fabs(e.decraw) });
    CHECK(MaxOffset(s) == Approx(included));
}

TEST_CASE("With every frame excluded, fitting uses them all", "[document]")
{
    Document doc = Load(OneSession(10, 2., 4.));
    doc.IncludeAll(false);
    CHECK(MaxOffset(*doc.Session()) == Approx(4.));
}

TEST_CASE("Zooming the vertical view scales the current session", "[document]")
{
    Document doc = Load(OneSession(10, 2., 4.));
    doc.FitVertical(300.);
    doc.ZoomVertical(2.);
    CHECK(doc.View()->vertical.Scale() == Approx(70.));
}

TEST_CASE("A locked vertical scale is the same in arc-seconds for every session", "[document]")
{
    // two sessions: 2"/px then 4"/px
    Document doc = Load(OneSession(10, 2., 4.) + OneSession(10, 4., 4.));
    doc.Select(0);
    doc.FitVertical(300.); // 35 px per pixel, 17.5 px per arc-second
    doc.LockVerticalScale(true);
    CHECK(doc.VerticalScaleLocked());
    CHECK(doc.VerticalLock() == Approx(17.5));

    doc.Select(1);
    CHECK(doc.View()->vertical.Scale() == Approx(70.));

    doc.ZoomVertical(2.);
    CHECK(doc.VerticalLock() == Approx(35.));
    doc.Select(0);
    CHECK(doc.View()->vertical.Scale() == Approx(70.));
}

TEST_CASE("Unlocking leaves every session at the locked scale", "[document]")
{
    Document doc = Load(OneSession(10, 2., 4.) + OneSession(10, 4., 4.));
    doc.SetVerticalLock(10.);
    doc.LockVerticalScale(false);
    CHECK_FALSE(doc.VerticalScaleLocked());
    doc.Select(0);
    CHECK(doc.View()->vertical.Scale() == Approx(20.));
    doc.Select(1);
    CHECK(doc.View()->vertical.Scale() == Approx(40.));
    doc.ZoomVertical(2.);
    doc.Select(0);
    CHECK(doc.View()->vertical.Scale() == Approx(20.));
}

TEST_CASE("Fitting while locked changes the locked scale", "[document]")
{
    Document doc = Load(OneSession(10, 2., 4.));
    doc.SetVerticalLock(1.);
    doc.FitVertical(300.);
    CHECK(doc.VerticalLock() == Approx(17.5));
}
