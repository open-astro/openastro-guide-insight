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

#include "nights.h"

#include "analysis.h"
#include "document.h"
#include "synthlog.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <math.h>
#include <sstream>

using Catch::Approx;
using synth::Frames;
using synth::GuidingHeader;
using synth::With;

namespace fs = std::filesystem;

namespace {

GuideLog Parse(const std::string& text)
{
    std::istringstream is(text);
    GuideLog log;
    LogParser parser;
    REQUIRE(parser.Parse(is, log));
    return log;
}

// A night's log: one guiding session starting at `start`, with random
// errors of `size` guide pixels.
std::string Night(const std::string& start, double size = 0.3,
                  std::vector<std::string> hdr = GuidingHeader())
{
    return synth::Log().Guiding(start, hdr, Frames(300, 2.5, size)).Text();
}

NightSummary Summary(const std::string& text, const std::string& path = "night.txt")
{
    auto s = SummarizeNight(Parse(text), path);
    REQUIRE(s);
    return *s;
}

std::string Value(const NightSummary& n, const std::string& setting)
{
    for (const auto& [name, value] : n.settings)
        if (name == setting)
            return value;
    return std::string();
}

bool Has(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

std::vector<std::string> DecMinMove(const char *value)
{
    return With(GuidingHeader(), "Y guide algorithm",
                std::string("Y guide algorithm = Resist Switch, Minimum move = ") + value +
                    " Aggression = 50% FastSwitch = enabled");
}

// A fresh, empty folder for one test.
struct TempDir
{
    fs::path path;
    explicit TempDir(const char *name)
    {
        path = fs::temp_directory_path() / (std::string("guideinsight-") + name);
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
    std::string Write(const std::string& file, const std::string& text) const
    {
        std::ofstream(path / file) << text;
        return (path / file).string();
    }
};

} // namespace

TEST_CASE("a night is summarised over its guiding", "[nights]")
{
    std::string text = Night("2026-10-04 21:00:00");
    NightSummary n = Summary(text);

    // the same RMS the statistics give, with the default exclusions
    GuideLog log = Parse(text);
    GuideSession& s = log.sessions[0];
    ApplyExclusions(s, ExclusionSettings());
    GuideStats st = CalcStats(s, 0, s.entries.size());
    CHECK(n.total == Approx(hypot(st.rms_ra, st.rms_dec) * 6.45));
    CHECK(n.frames == st.frames);
    CHECK(n.profile == "RedCat 51");
    CHECK(n.start == wxDateTime(4, wxDateTime::Oct, 2026, 21, 0, 0));
    CHECK(Value(n, "Exposure") == "2.0 s");
    CHECK(Value(n, "RA aggression") == "70%");
    CHECK(Value(n, "Dec minimum move") == "0.15 px");
    CHECK(Value(n, "Dec guide mode") == "Auto");
}

TEST_CASE("a log without guiding has no summary", "[nights]")
{
    CHECK_FALSE(SummarizeNight(Parse(synth::Log().Text()), "empty.txt"));
}

TEST_CASE("the settings that changed between nights are listed", "[nights]")
{
    NightSummary before = Summary(Night("2026-10-02 21:00:00", 0.3, DecMinMove("0.150")));
    NightSummary after = Summary(Night("2026-10-04 21:00:00", 0.3, DecMinMove("0.250")));
    auto changes = SettingChanges(before, after);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].setting == "Dec minimum move");
    CHECK(changes[0].from == "0.15 px");
    CHECK(changes[0].to == "0.25 px");
    CHECK(SettingChanges(before, before).empty());
}

TEST_CASE("tonight is compared with the latest earlier night on the same equipment", "[nights]")
{
    NightSummary older = Summary(Night("2026-09-28 21:00:00", 0.5), "a.txt");
    NightSummary last = Summary(Night("2026-10-02 21:00:00", 0.4, DecMinMove("0.150")), "b.txt");
    NightSummary otherGear = Summary(Night("2026-10-03 21:00:00", 0.6,
                                           With(GuidingHeader(), "Equipment Profile", "Equipment Profile = SCT")), "c.txt");
    NightSummary later = Summary(Night("2026-10-06 21:00:00", 0.2), "d.txt");
    NightSummary tonight = Summary(Night("2026-10-04 21:00:00", 0.3, DecMinMove("0.250")), "e.txt");

    auto c = CompareNights(tonight, { older, last, otherGear, later, tonight });
    REQUIRE(c);
    CHECK(c->earlier.path == "b.txt");
    CHECK(Has(c->text, "Since 2 October"));
    CHECK(Has(c->text, "→"));
    CHECK(Has(c->text, "%"));
    REQUIRE(c->changes.size() == 1);
    CHECK(c->changes[0].setting == "Dec minimum move");
    // the trend runs oldest first, up to tonight, on the same equipment
    CHECK(Has(c->trend, "Last 3 nights"));
}

TEST_CASE("unchanged settings put a difference down to the sky or the setup", "[nights]")
{
    NightSummary last = Summary(Night("2026-10-02 21:00:00", 0.4), "b.txt");
    NightSummary tonight = Summary(Night("2026-10-04 21:00:00", 0.3), "e.txt");
    auto c = CompareNights(tonight, { last });
    REQUIRE(c);
    CHECK(c->changes.empty());
    CHECK(Has(c->text, "No settings changed"));
}

TEST_CASE("with no earlier night there is nothing to compare", "[nights]")
{
    NightSummary tonight = Summary(Night("2026-10-04 21:00:00"), "e.txt");
    NightSummary later = Summary(Night("2026-10-06 21:00:00"), "f.txt");
    CHECK_FALSE(CompareNights(tonight, { tonight, later }));
}

TEST_CASE("the guide logs beside a log are found, newest first", "[nights]")
{
    TempDir dir("beside");
    std::string a = dir.Write("PHD2_GuideLog_2026-10-02_210000.txt", Night("2026-10-02 21:00:00"));
    std::string b = dir.Write("PHD2_GuideLog_2026-10-04_210000.txt", Night("2026-10-04 21:00:00"));
    std::string c = dir.Write("PHD2_GuideLog_2026-09-28_210000.txt", Night("2026-09-28 21:00:00"));
    dir.Write("PHD2_DebugLog_2026-10-04_210000.txt", "debug");
    dir.Write("notes.txt", "notes");

    auto logs = GuideLogsBeside(b);
    REQUIRE(logs.size() == 2);
    CHECK(fs::path(logs[0]).filename() == fs::path(a).filename());
    CHECK(fs::path(logs[1]).filename() == fs::path(c).filename());
    CHECK(GuideLogsBeside(b, 1).size() == 1);
}

TEST_CASE("logs are read once until they change", "[nights]")
{
    TempDir dir("cache");
    std::string a = dir.Write("PHD2_GuideLog_2026-10-02_210000.txt", Night("2026-10-02 21:00:00"));
    NightCache cache;
    auto first = cache.Summary(a);
    REQUIRE(first);
    cache.Summary(a);
    CHECK(cache.reads == 1);

    dir.Write("PHD2_GuideLog_2026-10-02_210000.txt", Night("2026-10-02 21:00:00", 0.6) + "\n");
    auto changed = cache.Summary(a);
    REQUIRE(changed);
    CHECK(cache.reads == 2);
    CHECK(changed->total > first->total);
}
