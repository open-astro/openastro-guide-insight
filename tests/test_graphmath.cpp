/*
 *  This file is part of phdlogview
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

#include "graphmath.h"
#include "logparser.h"

#include <catch2/catch_test_macros.hpp>

namespace {

// Builds entries from a pattern: 'o' = star found, 'x' = star lost.
GuideSession::EntryVec Entries(const std::string& pattern)
{
    GuideSession::EntryVec v;
    for (size_t i = 0; i < pattern.size(); i++)
    {
        GuideEntry e{};
        e.frame = (int) i + 1;
        e.err = pattern[i] == 'o' ? 0 : 2; // 2 = star lost (low SNR)
        v.push_back(e);
    }
    return v;
}

} // namespace

// Catch2 finds this by argument-dependent lookup, so it must share
// IndexRun's (global) namespace.
static bool operator==(const IndexRun& a, const IndexRun& b) { return a.first == b.first && a.last == b.last; }

TEST_CASE("All frames found draw as one line", "[graph]")
{
    auto runs = FoundRuns(Entries("ooooo"), 0, 4);
    REQUIRE(runs.size() == 1);
    CHECK(runs[0] == IndexRun{ 0, 4 });
}

TEST_CASE("Lost frames split the line into separate runs", "[graph]")
{
    auto runs = FoundRuns(Entries("ooxxooo"), 0, 6);
    REQUIRE(runs.size() == 2);
    CHECK(runs[0] == IndexRun{ 0, 1 });
    CHECK(runs[1] == IndexRun{ 4, 6 });
}

TEST_CASE("Lost frames at the edges are left out", "[graph]")
{
    auto runs = FoundRuns(Entries("xooox"), 0, 4);
    REQUIRE(runs.size() == 1);
    CHECK(runs[0] == IndexRun{ 1, 3 });
}

TEST_CASE("A single found frame between lost frames is its own run", "[graph]")
{
    auto runs = FoundRuns(Entries("xoxoo"), 0, 4);
    REQUIRE(runs.size() == 2);
    CHECK(runs[0] == IndexRun{ 1, 1 });
    CHECK(runs[1] == IndexRun{ 3, 4 });
}

TEST_CASE("No runs when every frame is lost", "[graph]")
{
    CHECK(FoundRuns(Entries("xxx"), 0, 2).empty());
}

TEST_CASE("Runs are limited to the visible range", "[graph]")
{
    // visible frames 2..5 of "ooxoooo"
    auto runs = FoundRuns(Entries("ooxoooo"), 2, 5);
    REQUIRE(runs.size() == 1);
    CHECK(runs[0] == IndexRun{ 3, 5 });
}

TEST_CASE("Saturated frames still count as found", "[graph]")
{
    auto entries = Entries("ooo");
    entries[1].err = 1; // saturated
    auto runs = FoundRuns(entries, 0, 2);
    REQUIRE(runs.size() == 1);
    CHECK(runs[0] == IndexRun{ 0, 2 });
}
