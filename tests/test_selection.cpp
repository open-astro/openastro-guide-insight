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

#include "analysis.h"
#include "logparser.h"
#include "selection.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>

using Catch::Approx;

namespace {

// n frames 2 s apart starting at 8 s, all included, star found. RA and Dec
// alternate +1 and -1 for frames [10, 20) and sit at 5 elsewhere.
GuideSession Session(unsigned int n)
{
    GuideSession s("2026-01-01 00:00:00");
    for (unsigned int i = 0; i < n; i++)
    {
        GuideEntry e{};
        e.frame = (int) i + 1;
        e.dt = 8.f + 2.f * (float) i;
        e.included = true;
        float v = i >= 10 && i < 20 ? (i % 2 ? -1.f : 1.f) : 5.f;
        e.raraw = v;
        e.decraw = v;
        s.entries.push_back(e);
    }
    return s;
}

std::string Pattern(const GuideSession& s)
{
    std::string p;
    for (const auto& e : s.entries)
        p += e.included ? 'o' : 'x';
    return p;
}

bool Same(const std::optional<IndexRun>& run, unsigned int first, unsigned int last)
{
    return run && run->first == first && run->last == last;
}

} // namespace

TEST_CASE("Statistics cover only the chosen frames", "[selection]")
{
    GuideSession s = Session(30);
    GuideStats st = CalcStats(s, 10, 20);
    CHECK(st.frames == 10);
    CHECK(st.rms_ra == Approx(1.));
    CHECK(st.rms_dec == Approx(1.));
    CHECK(st.peak_ra == Approx(1.));
}

TEST_CASE("Statistics skip excluded frames in the range", "[selection]")
{
    GuideSession s = Session(30);
    s.entries[12].included = false;
    s.entries[12].raraw = 100.f;
    GuideStats st = CalcStats(s, 10, 20);
    CHECK(st.frames == 9);
    CHECK(st.peak_ra == Approx(1.));
}

TEST_CASE("Statistics of the whole session match the session's own", "[selection]")
{
    std::ifstream ifs(std::string(TEST_DATA_DIR) + "/openastro_redcat51_2026-10-04.txt");
    GuideLog log;
    REQUIRE(LogParser().Parse(ifs, log));
    GuideSession& s = log.sessions[1];
    s.CalcStats();
    GuideStats st = CalcStats(s, 0, s.entries.size());
    CHECK(st.rms_ra == s.rms_ra);
    CHECK(st.rms_dec == s.rms_dec);
    CHECK(st.elongation == s.elongation);
    CHECK(st.drift_ra == s.drift_ra);
    CHECK(st.drift_dec == s.drift_dec);
    CHECK(st.paerr == s.paerr);
}

TEST_CASE("An empty range has no statistics", "[selection]")
{
    GuideSession s = Session(30);
    GuideStats st = CalcStats(s, 5, 5);
    CHECK(st.frames == 0);
    CHECK(st.rms_ra == 0.);
}

TEST_CASE("A range past the end stops at the last frame", "[selection]")
{
    GuideSession s = Session(30);
    CHECK(CalcStats(s, 25, 1000).frames == 5);
}

TEST_CASE("A drag on the frame axis selects the frames inside it", "[selection]")
{
    GuideSession s = Session(10);
    CHECK(Same(FramesBetween(s.entries, 2.3, 5.7, XAxis::Frames), 3, 5));
    CHECK(Same(FramesBetween(s.entries, 5.7, 2.3, XAxis::Frames), 3, 5));
    CHECK(Same(FramesBetween(s.entries, 3., 5., XAxis::Frames), 3, 5));
}

TEST_CASE("A drag past the ends selects up to the ends", "[selection]")
{
    GuideSession s = Session(10);
    CHECK(Same(FramesBetween(s.entries, -5., 2.5, XAxis::Frames), 0, 2));
    CHECK(Same(FramesBetween(s.entries, 7.5, 50., XAxis::Frames), 8, 9));
}

TEST_CASE("A drag between frames or off the session selects nothing", "[selection]")
{
    GuideSession s = Session(10);
    CHECK_FALSE(FramesBetween(s.entries, 3.2, 3.8, XAxis::Frames));
    CHECK_FALSE(FramesBetween(s.entries, 20., 30., XAxis::Frames));
    CHECK_FALSE(FramesBetween(s.entries, -3., -1., XAxis::Frames));
    CHECK_FALSE(FramesBetween(GuideSession::EntryVec(), 0., 5., XAxis::Frames));
}

TEST_CASE("A drag on the time axis selects the frames taken in it", "[selection]")
{
    GuideSession s = Session(10); // frames at 8, 10, 12 ... 26 s
    CHECK(Same(FramesBetween(s.entries, 11., 17., XAxis::Time), 2, 4));
    CHECK(Same(FramesBetween(s.entries, 0., 9., XAxis::Time), 0, 0));
    CHECK_FALSE(FramesBetween(s.entries, 30., 60., XAxis::Time));
    CHECK_FALSE(FramesBetween(s.entries, 12.5, 13.5, XAxis::Time));
}

TEST_CASE("Excluding a selection excludes its frames, ends included", "[selection]")
{
    GuideSession s = Session(8);
    IncludeFrames(s, IndexRun{ 2, 4 }, false);
    CHECK(Pattern(s) == "ooxxxooo");
    IncludeFrames(s, IndexRun{ 3, 3 }, true);
    CHECK(Pattern(s) == "ooxoxooo");
}
