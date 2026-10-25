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

#include "labels.h"

#include <catch2/catch_test_macros.hpp>

#include "comma_locale.h"

#include <cmath>
#include <fstream>
#include <string>

namespace {

GuideLog RealLog()
{
    std::ifstream ifs(TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt");
    GuideLog log;
    REQUIRE(LogParser().Parse(ifs, log));
    for (auto& s : log.sessions)
        s.CalcStats();
    return log;
}

GuideStats Stats()
{
    GuideStats st;
    st.frames = 149;
    st.rms_ra = 0.2;
    st.rms_dec = 0.15; // total 0.25 px
    st.peak_ra = 0.5;
    st.peak_dec = -1.;
    st.elongation = 0.183;
    st.drift_ra = 0.02;
    st.drift_dec = -0.1;
    st.paerr = 3.21;
    return st;
}

} // namespace

TEST_CASE("Numbers have a fixed number of decimals and a real minus sign", "[labels]")
{
    CHECK(Fixed(1.506, 2) == "1.51");
    CHECK(Fixed(-0.4, 2) == "−0.40");
    CHECK(Fixed(12., 0) == "12");
    CHECK(Fixed(-0.001, 2) == "0.00");
}

TEST_CASE("Signed numbers show their sign either way", "[labels]")
{
    CHECK(Signed(0.123, 2) == "+0.12");
    CHECK(Signed(-0.4, 2) == "−0.40");
    CHECK(Signed(0., 2) == "+0.00");
}

TEST_CASE("Numbers use a decimal point in every locale", "[labels]")
{
    CommaDecimalLocale comma;
    if (!comma.Active())
        SKIP("no locale with a decimal comma is installed");
    CHECK(Fixed(1.5, 1) == "1.5");
}

TEST_CASE("Durations read naturally", "[labels]")
{
    CHECK(Duration(45.2) == "45 s");
    CHECK(Duration(160.) == "2 min 40 s");
    CHECK(Duration(120.) == "2 min");
    CHECK(Duration(1662.) == "27 min 42 s");
    CHECK(Duration(3900.) == "1 h 5 min");
    CHECK(Duration(7200.) == "2 h");
    CHECK(Duration(0.) == "0 s");
}

TEST_CASE("Distances are shown in the chosen units", "[labels]")
{
    CHECK(Distance(0.25, 6.45, Units::ArcSeconds) == "1.61″");
    CHECK(Distance(0.25, 6.45, Units::Pixels) == "0.25 px");
    CHECK(Distance(-1., 2., Units::ArcSeconds, 1) == "−2.0″");
}

TEST_CASE("A guiding row shows its kind, start, length and RMS", "[labels]")
{
    GuideLog log = RealLog();
    SectionRow row = RowFor(log, 1, Units::ArcSeconds);
    CHECK_FALSE(row.calibration);
    CHECK(row.kind == "Guiding");
    CHECK(row.meta.rfind("07:06 · ", 0) == 0);
    CHECK(row.rms == Distance(hypot(log.sessions[0].rms_ra, log.sessions[0].rms_dec), log.sessions[0].pixelScale, Units::ArcSeconds));
}

TEST_CASE("A calibration row shows its start and steps", "[labels]")
{
    GuideLog log = RealLog();
    SectionRow row = RowFor(log, 0, Units::ArcSeconds);
    CHECK(row.calibration);
    CHECK(row.kind == "Calibration");
    CHECK(row.meta == "06:58 · " + std::to_string(log.calibrations[0].entries.size()) + " steps");
    CHECK(row.rms.empty());
}

TEST_CASE("A section's title is its kind and start time", "[labels]")
{
    GuideLog log = RealLog();
    CHECK(SectionTitle(log, 0) == "Calibration \u00b7 06:58:40");
    CHECK(SectionTitle(log, 1) == "Guiding \u00b7 07:06:47");
}

TEST_CASE("Combined sessions are titled by count and time span", "[labels]")
{
    GuideLog log = RealLog();
    // the second session starts at 07:12:08 and runs 27 min 42 s
    CHECK(CombinedTitle(log, { 1, 2 }) == "2 sessions \u00b7 07:06 \u2013 07:39");
}

TEST_CASE("The day heading names the day", "[labels]")
{
    wxDateTime t;
    t.ParseISOCombined("2026-10-04 07:06:47", ' ');
    CHECK(DayHeading(t) == "Sunday, October 4, 2026");
}

TEST_CASE("Total guiding adds up every session", "[labels]")
{
    GuideLog log = RealLog();
    CHECK(TotalGuiding(log) == log.sessions[0].duration + log.sessions[1].duration);
}

TEST_CASE("The summary is in the chosen units", "[labels]")
{
    SummaryText s = Summarize(Stats(), 160, 2., Units::ArcSeconds);
    CHECK(s.total == "0.50″");
    CHECK(s.alt == "0.25 px");
    CHECK(s.ra == "0.40″");
    CHECK(s.dec == "0.30″");
    CHECK(s.peak == "RA 1.00″ · Dec −2.00″");
    CHECK(s.elongation == "18%");
    CHECK(s.frames == "149 of 160 frames");
    CHECK(s.raDrift == "+0.04″/min");
    CHECK(s.decDrift == "−0.20″/min");
    CHECK(s.paError == "3.2′");

    SummaryText p = Summarize(Stats(), 160, 2., Units::Pixels);
    CHECK(p.total == "0.25 px");
    CHECK(p.alt == "0.50″");
    CHECK(p.raDrift == "+0.02 px/min");
}

TEST_CASE("Every frame used reads as a plain count", "[labels]")
{
    CHECK(Summarize(Stats(), 149, 2., Units::Pixels).frames == "149 frames");
}

namespace {

GuideSession HoverSession()
{
    GuideSession s("2026-10-04 07:06:47");
    s.starts.ParseISOCombined("2026-10-04 07:06:47", ' ');
    s.pixelScale = 2.;
    GuideEntry e{};
    e.frame = 53;
    e.dt = 10.4f;
    e.included = true;
    e.raraw = -0.015f;
    e.decraw = 0.06f;
    e.dx = 0.5f;
    e.dy = -0.25f;
    e.radur = -120;
    e.decdur = 0;
    e.snr = 52.13f;
    e.mass = 30411;
    s.entries.push_back(e);
    return s;
}

} // namespace

TEST_CASE("Hovering over a frame describes it", "[labels]")
{
    GuideSession s = HoverSession();
    CHECK(HoverText(s, 0, Units::ArcSeconds, true) ==
          "07:06:57 \u00b7 frame 53 \u00b7 RA \u22120.03\u2033 \u00b7 Dec 0.12\u2033 \u00b7 "
          "pulse RA 120 ms E \u00b7 SNR 52.1 \u00b7 mass 30411");
}

TEST_CASE("The hover text follows the units and axes", "[labels]")
{
    GuideSession s = HoverSession();
    s.entries[0].radur = 0;
    s.entries[0].decdur = -40;
    CHECK(HoverText(s, 0, Units::Pixels, false) ==
          "07:06:57 \u00b7 frame 53 \u00b7 dx 0.50 px \u00b7 dy \u22120.25 px \u00b7 "
          "pulse Dec 40 ms S \u00b7 SNR 52.1 \u00b7 mass 30411");
}

TEST_CASE("The hover text notes what is unusual about a frame", "[labels]")
{
    GuideSession s = HoverSession();
    GuideEntry& e = s.entries[0];
    e.radur = 0;
    e.err = 1; // saturated
    e.included = false;
    e.info = "DITHER by 3.402, -1.056";
    CHECK(HoverText(s, 0, Units::ArcSeconds, true) ==
          "07:06:57 \u00b7 frame 53 \u00b7 RA \u22120.03\u2033 \u00b7 Dec 0.12\u2033 \u00b7 "
          "SNR 52.1 \u00b7 mass 30411 \u00b7 saturated \u00b7 excluded \u00b7 DITHER by 3.402, -1.056");

    e.err = 2; // lost
    e.info.clear();
    e.included = true;
    CHECK(HoverText(s, 0, Units::ArcSeconds, true) == "07:06:57 \u00b7 frame 53 \u00b7 star lost");
}

TEST_CASE("The subtitle says how much of the session is in view", "[labels]")
{
    GuideSession s = HoverSession();
    s.duration = 1662.;
    CHECK(VisibleRange(s, 0., 1662.) == "Whole session");
    CHECK(VisibleRange(s, 653., 1373.) == "07:17:40 \u2013 07:29:40 of 27 min 42 s");
}

TEST_CASE("Copied statistics are CSV with a decimal point", "[labels]")
{
    CommaDecimalLocale comma;
    if (!comma.Active())
        SKIP("no locale with a decimal comma is installed");
    CHECK(StatsCsv(Stats(), 2.) ==
          "Statistic,arc-sec,pixels\n"
          "RA RMS,0.40,0.20\n"
          "Dec RMS,0.30,0.15\n"
          "Total RMS,0.50,0.25\n"
          "RA peak,1.00,0.50\n"
          "Dec peak,-2.00,-1.00\n"
          "Elongation (%),18,\n"
          "RA drift (per min),0.04,0.02\n"
          "Dec drift (per min),-0.20,-0.10\n"
          "Polar alignment error (arc-min),3.2,\n");
}
