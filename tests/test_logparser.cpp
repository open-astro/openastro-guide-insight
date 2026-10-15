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

#include "comma_locale.h"
#include "logparser.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <clocale>
#include <fstream>
#include <sstream>

using Catch::Matchers::WithinAbs;

namespace {

GuideLog ParseFile(const std::string& name)
{
    std::ifstream ifs(std::string(TEST_DATA_DIR) + "/" + name);
    REQUIRE(ifs.good());
    GuideLog log;
    REQUIRE(LogParser().Parse(ifs, log));
    return log;
}

GuideLog ParseText(const std::string& text)
{
    std::istringstream is(text);
    GuideLog log;
    LogParser().Parse(is, log);
    return log;
}

const char *GUIDING_PREAMBLE =
    "PHD2 version 2.6.13, Log version 2.5. Log enabled at 2026-01-01 00:00:00\n"
    "\n"
    "Guiding Begins at 2026-01-01 00:00:00\n"
    "Pixel scale = 1.00 arc-sec/px, Binning = 1, Focal length = 1000 mm\n"
    "Mount = Test Mount, connected, guiding enabled, xAngle = 0.0, xRate = 1.000, yAngle = 90.0, yRate = 1.000, parity = +/+\n"
    "Frame,Time,mount,dx,dy,RARawDistance,DECRawDistance,RAGuideDistance,DECGuideDistance,"
    "RADuration,RADirection,DECDuration,DECDirection,XStep,YStep,StarMass,SNR,ErrorCode\n";

} // namespace

TEST_CASE("Parses a real OpenAstro guider log", "[parser]")
{
    // Recorded by the OpenAstro headless guider (PHD2 2.1.0 on an ARM SBC).
    // Each header is preceded by an extra "OpenAstro Alpaca Support" line.
    GuideLog log = ParseFile("openastro_redcat51_2026-10-04.txt");

    CHECK(log.phd_version == "2.1.0 [Linux]");

    REQUIRE(log.sections.size() == 3);
    CHECK(log.sections[0].type == CALIBRATION_SECTION);
    CHECK(log.sections[1].type == GUIDING_SECTION);
    CHECK(log.sections[2].type == GUIDING_SECTION);

    REQUIRE(log.calibrations.size() == 1);
    CHECK(log.calibrations[0].date == "2026-10-04 06:58:40");
    CHECK(log.calibrations[0].entries.size() == 110);
    CHECK(log.calibrations[0].entries[0].direction == WEST);

    REQUIRE(log.sessions.size() == 2);
    const GuideSession& s0 = log.sessions[0];
    const GuideSession& s1 = log.sessions[1];

    CHECK(s0.date == "2026-10-04 07:06:47");
    CHECK(s1.date == "2026-10-04 07:12:08");
    CHECK(s0.entries.size() == 52);
    CHECK(s1.entries.size() == 661);
    CHECK_THAT(s0.pixelScale, WithinAbs(6.45, 1e-9));

    CHECK(s0.mount.isValid);
    CHECK_THAT(s0.mount.xRate, WithinAbs(0.822, 1e-9));
    CHECK_THAT(s0.mount.yRate, WithinAbs(1.045, 1e-9));

    const GuideEntry& e = s0.entries.front();
    CHECK(e.frame == 1);
    CHECK_THAT(e.dt, WithinAbs(8.408, 1e-4));
    CHECK(e.mount == MOUNT);
    CHECK_THAT(e.raraw, WithinAbs(1.601, 1e-4));
    CHECK_THAT(e.decraw, WithinAbs(6.315, 1e-4));
    // Durations are signed by direction: West and South are negative.
    CHECK(e.radur == -1227);  // 1227 ms West
    CHECK(e.decdur == -2500); // 2500 ms South
    CHECK(e.mass == 326811);
    CHECK(e.err == 0);
    CHECK(e.included);

    CHECK(s1.entries.back().frame == 661);
    CHECK_THAT(s1.entries.back().dt, WithinAbs(1662.285, 1e-3));

    bool sawDither = false;
    for (const auto& info : s1.infos)
        if (info.info.rfind("DITHER", 0) == 0)
            sawDither = true;
    CHECK(sawDither);
}

TEST_CASE("A log with only a header has no sections", "[parser]")
{
    // The guider writes one of these every time it starts without guiding.
    GuideLog log = ParseText(
        "OpenAstro Alpaca Support\n"
        "PHD2 version 2.1.0 [Linux], Log version 2.5. Log enabled at 2026-10-13 15:32:11\n"
        "INFO: Guiding parameter change, MultiStar = false\n"
        "INFO: Guiding parameter change, MultiStar = false\n");

    CHECK(log.phd_version == "2.1.0 [Linux]");
    CHECK(log.sections.empty());
    CHECK(log.sessions.empty());
    CHECK(log.calibrations.empty());
}

TEST_CASE("Empty input parses to an empty log", "[parser]")
{
    GuideLog log = ParseText("");
    CHECK(log.phd_version.empty());
    CHECK(log.sections.empty());
}

TEST_CASE("Frames where the star was lost are excluded", "[parser]")
{
    GuideLog log = ParseText(std::string(GUIDING_PREAMBLE) +
        "1,2.000,\"Mount\",0.1,0.1,0.1,0.1,0.1,0.1,100,W,100,N,,,50000,30.00,0\n"
        "2,4.000,\"Mount\",,,,,,,,,,,,,0,0.00,2,\"Star lost - low SNR\"\n"
        "3,6.000,\"Mount\",0.2,0.2,0.2,0.2,0.2,0.2,200,W,200,N,,,50000,30.00,1\n");

    REQUIRE(log.sessions.size() == 1);
    const auto& entries = log.sessions[0].entries;
    REQUIRE(entries.size() == 3);
    CHECK(entries[0].included);
    CHECK_FALSE(entries[1].included);
    CHECK(entries[1].err == 2);
    CHECK(entries[1].info == "Star lost - low SNR");
    CHECK(entries[2].included); // 1 = saturated, which still counts as found
}

TEST_CASE("Backwards timestamp jumps are repaired", "[parser]")
{
    GuideLog log = ParseText(std::string(GUIDING_PREAMBLE) +
        "1,2.000,\"Mount\",0,0,0,0,0,0,0,,0,,,,50000,30.00,0\n"
        "2,4.000,\"Mount\",0,0,0,0,0,0,0,,0,,,,50000,30.00,0\n"
        "3,6.000,\"Mount\",0,0,0,0,0,0,0,,0,,,,50000,30.00,0\n"
        "4,1.000,\"Mount\",0,0,0,0,0,0,0,,0,,,,50000,30.00,0\n"
        "5,3.000,\"Mount\",0,0,0,0,0,0,0,,0,,,,50000,30.00,0\n"
        "6,5.000,\"Mount\",0,0,0,0,0,0,0,,0,,,,50000,30.00,0\n");

    REQUIRE(log.sessions.size() == 1);
    const GuideSession& s = log.sessions[0];
    REQUIRE(s.entries.size() == 6);

    // The backwards step is replaced by the median interval of 2 s.
    const double expected[] = { 2., 4., 6., 8., 10., 12. };
    for (size_t i = 0; i < 6; i++)
        CHECK_THAT(s.entries[i].dt, WithinAbs(expected[i], 1e-4));

    bool flagged = false;
    for (const auto& info : s.infos)
        if (info.info == "Timestamp jumped backwards")
            flagged = true;
    CHECK(flagged);
}

TEST_CASE("Progress callback is invoked on long logs", "[parser]")
{
    std::ifstream ifs(std::string(TEST_DATA_DIR) + "/openastro_redcat51_2026-10-04.txt");
    REQUIRE(ifs.good());

    LogParser parser;
    int calls = 0;
    parser.progress = [&calls] { ++calls; };
    GuideLog log;
    parser.Parse(ifs, log);

    CHECK(calls > 0);
}

TEST_CASE("A PHD2 debug log is recognised", "[parser]")
{
    // First lines of a real debug log from the OpenAstro guider. Each line is
    // prefixed with the time, the time since the previous line, and a thread id.
    GuideLog log = ParseText(
        "15:32:11.204 00.000 2041 OpenAstro Alpaca Support\n"
        "15:32:11.205 00.001 2041 PHD2 version 2.1.0 begins execution with:\n"
        "15:32:11.205 00.000 2041    Linux 6.12.47+rpt-rpi-2712 aarch64\n"
        "15:32:11.206 00.001 2041    wxWidgets 3.2.6\n");

    CHECK(log.is_debug_log);
    CHECK(log.sections.empty());
}

TEST_CASE("A guide log is not mistaken for a debug log", "[parser]")
{
    CHECK_FALSE(ParseFile("openastro_redcat51_2026-10-04.txt").is_debug_log);
    CHECK_FALSE(ParseText("").is_debug_log);
}


TEST_CASE("Numbers parse the same when the system uses a decimal comma", "[parser]")
{
    // Upstream issue #12: with a comma decimal separator, values were cut off
    // at the decimal point. PHD2 always writes a decimal point.
    CommaDecimalLocale comma;
    if (!comma.Active())
        SKIP("no locale with a decimal comma is installed");

    GuideLog log = ParseFile("openastro_redcat51_2026-10-04.txt");

    REQUIRE(log.sessions.size() == 2);
    const GuideSession& s0 = log.sessions[0];
    CHECK_THAT(s0.pixelScale, WithinAbs(6.45, 1e-9));
    CHECK_THAT(s0.mount.xRate, WithinAbs(0.822, 1e-9));
    CHECK_THAT(s0.entries.front().dt, WithinAbs(8.408, 1e-4));
    CHECK_THAT(s0.entries.front().raraw, WithinAbs(1.601, 1e-4));
    CHECK_THAT(s0.entries.front().snr, WithinAbs(52.15, 1e-3));
}
