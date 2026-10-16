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

#include "analysis.h"
#include "logparser.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "comma_locale.h"

#include <cmath>
#include <sstream>
#include <fstream>

using Catch::Matchers::WithinAbs;

namespace {

GuideLog LoadRealLog()
{
    std::ifstream ifs(std::string(TEST_DATA_DIR) + "/openastro_redcat51_2026-10-04.txt");
    REQUIRE(ifs.good());
    GuideLog log;
    LogParser().Parse(ifs, log);
    REQUIRE(log.sessions.size() == 2);
    return log;
}

GuideEntry MakeEntry(int frame, double t, double raraw, double decraw)
{
    GuideEntry e{};
    e.frame = frame;
    e.dt = (float) t;
    e.mount = MOUNT;
    e.included = true;
    e.guiding = true;
    e.raraw = (float) raraw;
    e.decraw = (float) decraw;
    e.err = 0;
    return e;
}

// Unguided session whose RA error is a sine wave of the given period plus an
// optional linear drift, sampled every 2 seconds.
GuideSession SineSession(size_t n, double period, double amplitude, double drift_per_sec)
{
    GuideSession s("2026-01-01 00:00:00");
    for (size_t i = 0; i < n; i++)
    {
        double t = 2.0 * (double) i;
        double ra = amplitude * sin(2. * M_PI * t / period) + drift_per_sec * t;
        s.entries.push_back(MakeEntry((int) i + 1, t, ra, 0.));
    }
    return s;
}

size_t PeakIndex(const GARun& ga)
{
    size_t best = 0;
    for (size_t i = 1; i < ga.nfft; i++)
        if (ga.ffty[i] > ga.ffty[best])
            best = i;
    return best;
}

} // namespace

TEST_CASE("Session statistics match an independent calculation", "[analysis]")
{
    // Expected values were computed separately from the raw log rows
    // (population standard deviation of RARawDistance / DECRawDistance).
    GuideLog log = LoadRealLog();

    GuideSession& s0 = log.sessions[0];
    s0.CalcStats();
    CHECK_THAT(s0.rms_ra, WithinAbs(0.292506, 1e-4));
    CHECK_THAT(s0.rms_dec, WithinAbs(1.029488, 1e-4));
    CHECK_THAT(s0.peak_ra, WithinAbs(1.601, 1e-4));
    CHECK_THAT(s0.peak_dec, WithinAbs(6.315, 1e-4));

    GuideSession& s1 = log.sessions[1];
    s1.CalcStats();
    CHECK_THAT(s1.rms_ra, WithinAbs(0.346399, 1e-4));
    CHECK_THAT(s1.rms_dec, WithinAbs(0.431790, 1e-4));
    CHECK_THAT(s1.peak_ra, WithinAbs(-4.116, 1e-4));
    CHECK_THAT(s1.peak_dec, WithinAbs(3.035, 1e-4));

    CHECK(s1.elongation >= 0.);
    CHECK(s1.elongation <= 1.);
    CHECK(std::isfinite(s1.drift_ra));
    CHECK(std::isfinite(s1.drift_dec));
    CHECK(std::isfinite(s1.paerr));
}

TEST_CASE("Session with no usable frames has zero statistics", "[analysis]")
{
    GuideSession s("2026-01-01 00:00:00");
    s.CalcStats();
    CHECK(s.rms_ra == 0.);
    CHECK(s.rms_dec == 0.);
    CHECK(s.drift_ra == 0.);
    CHECK(s.drift_dec == 0.);
}

TEST_CASE("Session where every frame lost the star has zero statistics", "[analysis]")
{
    GuideSession s("2026-01-01 00:00:00");
    for (int i = 0; i < 5; i++)
    {
        GuideEntry e = MakeEntry(i + 1, 2. * i, 1. + i, 2. + i);
        e.included = false;
        e.err = 2; // star lost
        s.entries.push_back(e);
    }
    s.CalcStats();
    CHECK(s.rms_ra == 0.);
    CHECK(s.rms_dec == 0.);
    CHECK(s.drift_ra == 0.);
    CHECK(s.drift_dec == 0.);
    CHECK(s.paerr == 0.);
}

TEST_CASE("Guiding Assistant analysis needs at least 12 frames", "[analysis]")
{
    GuideSession s = SineSession(11, 120., 1., 0.);
    CHECK_FALSE(GARun::CanAnalyze(s, 0, s.entries.size()));

    s.entries.push_back(MakeEntry(12, 22., 0., 0.));
    CHECK(GARun::CanAnalyze(s, 0, s.entries.size()));
}

TEST_CASE("Periodogram finds the period of a sine wave", "[analysis]")
{
    // 480 samples over 960 s: a 120 s period falls exactly on an FFT bin.
    GuideSession s = SineSession(481, 120., 2., 0.);

    GARun ga;
    ga.Analyze(s, 0, s.entries.size(), false);

    REQUIRE(ga.len == 481);
    REQUIRE(ga.nfft == 481 / 2 - 1);
    CHECK_THAT(ga.fftx[PeakIndex(ga)], WithinAbs(120., 1.));
    CHECK(ga.fftymax == ga.ffty[PeakIndex(ga)]);
}

TEST_CASE("Linear drift is removed before the periodogram", "[analysis]")
{
    // A strong drift would dominate the longest periods if it were not removed.
    GuideSession s = SineSession(481, 120., 2., 0.05);

    GARun ga;
    ga.Analyze(s, 0, s.entries.size(), false);

    CHECK_THAT(ga.fftx[PeakIndex(ga)], WithinAbs(120., 1.));

    // Drift-corrected RA should have no remaining trend.
    double first = ga.rac[0], last = ga.rac[ga.len - 1];
    CHECK(fabs(last - first) < 1.0);
}

TEST_CASE("Guiding Assistant analysis runs on a real session", "[analysis]")
{
    GuideLog log = LoadRealLog();
    const GuideSession& s = log.sessions[1];

    REQUIRE(GARun::CanAnalyze(s, 0, s.entries.size()));
    GARun ga;
    ga.Analyze(s, 0, s.entries.size(), true);

    REQUIRE(ga.len == s.entries.size());
    REQUIRE(ga.nfft == ga.len / 2 - 1);
    for (size_t i = 0; i < ga.nfft; i++)
    {
        CHECK(std::isfinite(ga.fftx[i]));
        CHECK(std::isfinite(ga.ffty[i]));
        CHECK(ga.fftx[i] > 0.);
    }
    for (size_t i = 1; i < ga.nfft; i++)
        CHECK(ga.fftx[i] > ga.fftx[i - 1]); // periods ascend
}

TEST_CASE("Frequency analysis CSV uses a decimal point in every locale", "[analysis]")
{
    GuideSession s = SineSession(481, 120., 2., 0.);
    GARun ga;
    ga.Analyze(s, 0, s.entries.size(), false);
    ga.pixscale = 6.45;

    CommaDecimalLocale comma;
    if (!comma.Active())
        SKIP("no locale with a decimal comma is installed");

    std::istringstream csv(FftCsv(ga));
    std::string line;
    REQUIRE(std::getline(csv, line));
    CHECK(line == "Period (s),Amplitude (px),Amplitude (\")");

    size_t rows = 0;
    while (std::getline(csv, line))
    {
        ++rows;
        // three fields: period, amplitude in pixels, amplitude in arc-seconds
        REQUIRE(std::count(line.begin(), line.end(), ',') == 2);
    }
    CHECK(rows == ga.nfft);

    std::istringstream first(FftCsv(ga));
    std::getline(first, line); // header
    std::getline(first, line);
    CHECK(line.find('.') != std::string::npos);
}

namespace {

// A session of n frames with guide output on or off for each frame
// ('g' = guided, 'u' = unguided), optional header lines and infos.
GuideSession Labelled(const std::string& guided, std::initializer_list<const char *> hdr = {},
                      std::initializer_list<const char *> infos = {})
{
    GuideSession s("2026-01-01 00:00:00");
    for (size_t i = 0; i < guided.size(); i++)
    {
        GuideEntry e = MakeEntry((int) i + 1, 2. * i, 0., 0.);
        e.guiding = guided[i] == 'g';
        s.entries.push_back(e);
    }
    for (const char *h : hdr)
        s.hdr.Add(h);
    for (const char *text : infos)
    {
        InfoEntry ie;
        ie.idx = (int) s.entries.size();
        ie.repeats = 1;
        ie.info = text;
        s.infos.push_back(ie);
    }
    return s;
}

} // namespace

TEST_CASE("Ordinary guiding is labelled Guiding", "[sections]")
{
    CHECK(SessionLabel(Labelled("gggg", { "Max RA duration = 2500, Max DEC duration = 2500, DEC guide mode = Auto" })) ==
          "Guiding");
}

TEST_CASE("A Guiding Assistant run is labelled", "[sections]")
{
    // Upstream issue #7. GA turns guide output off and logs its results.
    auto ga = Labelled("uuuu", {}, { "GA Result - Dec Drift Rate=0.3 a-s/min, Dec Peak=1.2 a-s, PA Error=2.1 a-m" });
    CHECK(SessionLabel(ga) == "Guiding Assistant");
}

TEST_CASE("Guiding that includes a Guiding Assistant run is labelled", "[sections]")
{
    auto mixed = Labelled("gguuuugg", {}, { "GA Result - RA Peak=3.1 a-s" });
    CHECK(SessionLabel(mixed) == "Guiding + GA");
}

TEST_CASE("A session with guide output off throughout is labelled Unguided", "[sections]")
{
    CHECK(SessionLabel(Labelled("uuu")) == "Unguided");
}

TEST_CASE("A session with Dec guiding off is labelled RA only", "[sections]")
{
    // Drift alignment turns Dec guiding off. PHD2 only records which
    // drift-align phase it was in the debug log, so Alt/Az can't be shown.
    auto raOnly = Labelled("gggg", { "Max RA duration = 2500, Max DEC duration = 2500, DEC guide mode = Off" });
    CHECK(SessionLabel(raOnly) == "Guiding, RA only");
}

TEST_CASE("A session with no frames is labelled Guiding", "[sections]")
{
    CHECK(SessionLabel(Labelled("")) == "Guiding");
}

TEST_CASE("The real log's sessions are ordinary guiding", "[sections]")
{
    GuideLog log = LoadRealLog();
    CHECK(SessionLabel(log.sessions[0]) == "Guiding");
    CHECK(SessionLabel(log.sessions[1]) == "Guiding");
}

TEST_CASE("Drift correction can be turned off", "[analysis]")
{
    // Upstream issue #10. 0.05 px/s of drift over 960 s is 48 px.
    GuideSession s = SineSession(481, 120., 2., 0.05);

    GARun corrected;
    corrected.Analyze(s, 0, s.entries.size(), false);
    CHECK(fabs(corrected.rac[corrected.len - 1] - corrected.rac[0]) < 1.0);

    GARun raw;
    raw.Analyze(s, 0, s.entries.size(), false, false);
    CHECK_THAT(raw.rac[raw.len - 1] - raw.rac[0], WithinAbs(48., 0.5));
    CHECK(raw.decc[0] == s.entries[0].decraw);
}
