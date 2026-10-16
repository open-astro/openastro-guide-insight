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

#include "exclusion.h"
#include "logparser.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

// n frames 2 s apart, all included, star found, offset (dx, dy) = (5, 5).
GuideSession Session(unsigned int n)
{
    GuideSession s("2026-01-01 00:00:00");
    for (unsigned int i = 0; i < n; i++)
    {
        GuideEntry e{};
        e.frame = (int) i + 1;
        e.dt = 2.f * (float) i;
        e.included = true;
        e.dx = 5.f;
        e.dy = 5.f;
        s.entries.push_back(e);
    }
    return s;
}

void AddInfo(GuideSession& s, int idx, const std::string& text)
{
    InfoEntry e;
    e.idx = idx;
    e.repeats = 1;
    e.info = text;
    s.infos.push_back(e);
}

// "o" for included frames, "x" for excluded, e.g. "ooxxxoo".
std::string Pattern(const GuideSession& s)
{
    std::string p;
    for (const auto& e : s.entries)
        p += e.included ? 'o' : 'x';
    return p;
}

} // namespace

// Characterization tests: these pin down the settling exclusions as they
// behaved when they moved out of LogViewFrame.cpp.

TEST_CASE("Settling reported by the server is excluded", "[exclusion]")
{
    GuideSession s = Session(10);
    AddInfo(s, 2, "Settling started");
    AddInfo(s, 5, "Settling complete");
    ExcludeSettlingByServer(s);
    CHECK(Pattern(s) == "ooxxxooooo");
}

TEST_CASE("Failed settling also ends the excluded range", "[exclusion]")
{
    GuideSession s = Session(8);
    AddInfo(s, 1, "Settling started");
    AddInfo(s, 3, "Settling failed");
    ExcludeSettlingByServer(s);
    CHECK(Pattern(s) == "oxxooooo");
}

TEST_CASE("Settling that never completes is excluded to the end", "[exclusion]")
{
    GuideSession s = Session(6);
    AddInfo(s, 4, "Settling started");
    ExcludeSettlingByServer(s);
    CHECK(Pattern(s) == "ooooxx");
}

TEST_CASE("Settling by distance excludes from the dither until settled", "[exclusion]")
{
    // Dither at frame 2; from frame 5 the star stays within 1 px. With a
    // 3 s settle time it counts as settled at frame 7 (4 s later).
    GuideSession s = Session(10);
    for (unsigned int i = 5; i < 10; i++)
        s.entries[i].dx = s.entries[i].dy = 0.1f;
    AddInfo(s, 2, "DITHER by 3.000, 3.000");
    ExcludeSettlingByDistance(s, SettleParams{ 1.0, 3.0 });
    CHECK(Pattern(s) == "ooxxxxxooo");
}

TEST_CASE("A dither that never settles by distance excludes nothing", "[exclusion]")
{
    // Existing behaviour: the range is only excluded once settling is seen.
    GuideSession s = Session(6);
    AddInfo(s, 1, "DITHER by 3.000, 3.000");
    ExcludeSettlingByDistance(s, SettleParams{ 1.0, 3.0 });
    CHECK(Pattern(s) == "oooooo");
}

TEST_CASE("Include and exclude ranges of frames", "[exclusion]")
{
    GuideSession s = Session(5);
    IncludeRange(s.entries, false, 1, 3);
    CHECK(Pattern(s) == "oxxoo");
    IncludeRange(s.entries, true);
    CHECK(Pattern(s) == "ooooo");
    IncludeRange(s.entries, false, 3);
    CHECK(Pattern(s) == "oooxx");
}

TEST_CASE("Frames while the server paused guiding are excluded", "[exclusion]")
{
    // Upstream issue #17: exclude the period between PAUSE and RESUME so
    // the corrections that recover the star are left out of the statistics.
    GuideSession s = Session(10);
    AddInfo(s, 3, "Server received PAUSE");
    AddInfo(s, 6, "Server received RESUME");
    ExcludeServerPauses(s);
    CHECK(Pattern(s) == "oooxxxoooo");
}

TEST_CASE("A pause with no resume is excluded to the end", "[exclusion]")
{
    GuideSession s = Session(6);
    AddInfo(s, 4, "Server received PAUSE");
    ExcludeServerPauses(s);
    CHECK(Pattern(s) == "ooooxx");
}

TEST_CASE("Several pauses are each excluded", "[exclusion]")
{
    GuideSession s = Session(10);
    AddInfo(s, 1, "Server received PAUSE");
    AddInfo(s, 2, "Server received RESUME");
    AddInfo(s, 6, "Server received PAUSE");
    AddInfo(s, 8, "Server received RESUME");
    ExcludeServerPauses(s);
    CHECK(Pattern(s) == "oxooooxxoo");
}

TEST_CASE("Other server commands do not exclude frames", "[exclusion]")
{
    GuideSession s = Session(5);
    AddInfo(s, 1, "Server received LOOP");
    AddInfo(s, 2, "Server received RESUME");
    ExcludeServerPauses(s);
    CHECK(Pattern(s) == "ooooo");
}
