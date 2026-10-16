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

// The assessment's checks, each against a synthetic guide log with the
// problem it looks for and one without.

#include "assessment.h"
#include "document.h"
#include "synthlog.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

using synth::Frames;
using synth::GuidingHeader;
using synth::With;

namespace {

const char *const START = "2026-10-04 07:12:08";

// The assessment of the log's last section, which is a guiding session.
Assessment AssessLog(const std::string& text)
{
    std::istringstream is(text);
    Document doc;
    REQUIRE(doc.Load(is));
    REQUIRE_FALSE(doc.Log().sections.empty());
    doc.Select(doc.Log().sections.size() - 1);
    const GuideSession *s = doc.Session();
    REQUIRE(s);
    return Assess(doc.Log(), *s, 0, s->entries.size(), doc.Stats());
}

Assessment AssessGuiding(const std::vector<std::string>& hdr, const std::vector<synth::Frame>& frames,
                         const std::multimap<size_t, std::string>& infos = {})
{
    return AssessLog(synth::Log().Guiding(START, hdr, frames, infos).Text());
}

const Finding *Find(const std::vector<Finding>& findings, const std::string& title)
{
    for (const Finding& f : findings)
        if (f.title.rfind(title, 0) == 0)
            return &f;
    return nullptr;
}

bool Has(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

// RA pulses on every other frame, `east` of every 10 of them eastward.
std::vector<synth::Frame> RaPulses(int east)
{
    auto frames = Frames(300);
    for (size_t i = 0; i < frames.size(); i += 2)
        frames[i].raPulse = (int) ((i / 2) % 10) < east ? 200 : -200;
    return frames;
}

} // namespace

TEST_CASE("a synthetic log reads back as the session it describes", "[catalogue]")
{
    std::istringstream is(synth::Log().Guiding(START, GuidingHeader(), Frames(300)).Text());
    Document doc;
    REQUIRE(doc.Load(is));
    REQUIRE(doc.Log().sessions.size() == 1);
    CHECK(doc.Log().sessions[0].entries.size() == 300);
    CHECK(doc.Log().sessions[0].pixelScale == 6.45);
}

TEST_CASE("mounts are told apart by name", "[catalogue]")
{
    CHECK(KindOfMount("Alpaca Mount [localhost:6800/0] - iOptron HAE29C EQ, connected") == MountKind::StrainWave);
    CHECK(KindOfMount("ZWO AM5") == MountKind::StrainWave);
    CHECK(KindOfMount("Rainbow Astro RST-135") == MountKind::StrainWave);
    CHECK(KindOfMount("Pegasus Astro NYX-101") == MountKind::StrainWave);
    CHECK(KindOfMount("EQMOD HEQ5/6, Sky-Watcher EQ6-R Pro") == MountKind::Geared);
    CHECK(KindOfMount("iOptron CEM40") == MountKind::Geared);
    CHECK(KindOfMount("Celestron CGX") == MountKind::Geared);
    CHECK(KindOfMount("On Camera") == MountKind::Unknown);
}

TEST_CASE("RA pushed mostly one way is flagged, with balance advice only for geared mounts", "[catalogue]")
{
    Assessment a = AssessGuiding(GuidingHeader(), RaPulses(9));
    const Finding *f = Find(a.improve, "RA is pushed mostly one way");
    REQUIRE(f);
    CHECK(Has(f->body, "east"));
    CHECK_FALSE(Has(f->body, "balance")); // the HAE29C has no counterweight to balance

    auto geared = With(GuidingHeader(), "Mount = ",
                       "Mount = EQMOD HEQ5/6 - Sky-Watcher EQ6-R Pro, connected, guiding enabled, xAngle = -114.5, xRate = 0.822, yAngle = -23.6, yRate = 1.045, parity = +/+");
    Assessment g = AssessGuiding(geared, RaPulses(9));
    f = Find(g.improve, "RA is pushed mostly one way");
    REQUIRE(f);
    CHECK(Has(f->body, "balance"));

    CHECK_FALSE(Find(AssessGuiding(GuidingHeader(), RaPulses(5)).improve, "RA is pushed mostly one way"));
}

TEST_CASE("a calibration with few steps is flagged", "[catalogue]")
{
    auto text = [](int steps) {
        return synth::Log()
            .Calibration("2026-10-04 06:58:40", steps, steps, steps, steps)
            .Guiding(START, GuidingHeader(), Frames(300))
            .Text();
    };
    const Finding *f = nullptr;
    Assessment a = AssessLog(text(5));
    f = Find(a.improve, "Calibration was short");
    REQUIRE(f);
    CHECK(Has(f->body, "5 steps west"));
    CHECK_FALSE(Find(AssessLog(text(14)).improve, "Calibration was short"));
}

TEST_CASE("calibration rates far from the guide speed are flagged", "[catalogue]")
{
    auto slow = With(GuidingHeader(), "Norm rates", "Norm rates RA = 2.1\"/s @ dec 0, Dec = 6.7\"/s; ortho.err. = 0.9 deg");
    const Finding *f = nullptr;
    Assessment a = AssessGuiding(slow, Frames(300));
    f = Find(a.improve, "Calibration doesn't match the guide speed");
    REQUIRE(f);
    CHECK(Has(f->body, "2.1"));
    CHECK_FALSE(Find(AssessGuiding(GuidingHeader(), Frames(300)).improve, "Calibration doesn't match"));
}

TEST_CASE("the minimum move follows PHD2's rule when a Guiding Assistant measured the noise", "[catalogue]")
{
    std::multimap<size_t, std::string> ga{ { 5, "GA Result - SNR=40.1, Samples=120, Elapsed Time=  120 s, "
                                                "RA HPF-RMS=  0.18 px (  1.16 arc-sec ), Dec HPF-RMS=  0.20 px "
                                                "(  1.29 arc-sec ), Total HPF-RMS=  0.27 px (  1.74 arc-sec )" } };
    Assessment a = AssessGuiding(GuidingHeader(), Frames(300), ga);
    const Finding *f = Find(a.improve, "Minimum move doesn't match the measured noise");
    REQUIRE(f);
    REQUIRE(f->changes.size() == 2);
    // Dec: 0.20 px x 1.65 (guide pixels over 1.5"); RA: 65% of Dec
    CHECK(f->changes[0].setting == "RA minimum move");
    CHECK(f->changes[0].to == "0.21 px");
    CHECK(f->changes[1].setting == "Dec minimum move");
    CHECK(f->changes[1].to == "0.33 px");

    auto matched = With(With(GuidingHeader(), "X guide algorithm",
                             "X guide algorithm = Hysteresis, Hysteresis = 0.100, Aggression = 0.700, Minimum move = 0.210"),
                        "Y guide algorithm", "Y guide algorithm = Resist Switch, Minimum move = 0.330 Aggression = 50% FastSwitch = enabled");
    CHECK_FALSE(Find(AssessGuiding(matched, Frames(300), ga).improve, "Minimum move doesn't match"));
}

TEST_CASE("a slow guide speed is flagged", "[catalogue]")
{
    auto slow = With(GuidingHeader(), "RA Guide Speed",
                     "RA Guide Speed = 3.8 a-s/s, Dec Guide Speed = 3.8 a-s/s, Cal Dec = 44.5, Last Cal Issue = None");
    const Finding *f = nullptr;
    Assessment a = AssessGuiding(slow, Frames(300));
    f = Find(a.improve, "Guide speed is slow");
    REQUIRE(f);
    REQUIRE(f->changes.size() == 1);
    CHECK(f->changes[0].from == "0.25× sidereal");
    CHECK_FALSE(Find(AssessGuiding(GuidingHeader(), Frames(300)).improve, "Guide speed is slow"));
}

TEST_CASE("Dec pushed only one way in Auto mode can guide in that direction only", "[catalogue]")
{
    auto frames = Frames(300);
    for (size_t i = 0; i < frames.size(); i += 3)
        frames[i].decPulse = -150; // south
    Assessment a = AssessGuiding(GuidingHeader(), frames);
    const Finding *f = Find(a.improve, "Dec only needs pushing one way");
    REQUIRE(f);
    REQUIRE(f->changes.size() == 1);
    CHECK(f->changes[0].setting == "Dec guide mode");
    CHECK(f->changes[0].from == "Auto");
    CHECK(f->changes[0].to == "South");

    auto south = With(GuidingHeader(), "Max RA duration", "Max RA duration = 2500, Max DEC duration = 2500, DEC guide mode = South");
    CHECK_FALSE(Find(AssessGuiding(south, frames).improve, "Dec only needs pushing one way"));
}

TEST_CASE("very small guide pixels suggest binning", "[catalogue]")
{
    auto fine = With(GuidingHeader(), "Pixel scale", "Pixel scale = 0.40 arc-sec/px, Binning = 1, Focal length = 1960 mm");
    const Finding *f = nullptr;
    Assessment a = AssessGuiding(fine, Frames(300, 2.5, 3.));
    f = Find(a.improve, "Bin the guide camera");
    REQUIRE(f);
    REQUIRE(f->changes.size() == 1);
    CHECK(f->changes[0].from == "1");
    CHECK(f->changes[0].to == "2");
    CHECK_FALSE(Find(AssessGuiding(GuidingHeader(), Frames(300)).improve, "Bin the guide camera"));
}

TEST_CASE("dither size is put in imaging pixels", "[catalogue]")
{
    auto hdr = With(GuidingHeader(), "Imaging camera",
                    "Imaging camera = Uranus-C PRO, Pixel size = 2.90 x 2.90 um, Binning = 1x1, Pixel scale = 2.39 arc-sec/px");
    auto dither = [&](const char *by) {
        return AssessGuiding(hdr, Frames(300), { { 100, by }, { 100, "SETTLING STATE CHANGE, Settling started" },
                                                 { 104, "SETTLING STATE CHANGE, Settling complete" } });
    };
    Assessment ok = dither("DITHER by 4.116, -3.024");
    REQUIRE(Find(ok.good, "Dither size"));
    CHECK(Has(Find(ok.good, "Dither size")->body, "14 imaging pixels"));

    Assessment small = dither("DITHER by 0.500, 0.300");
    CHECK(Find(small.improve, "Dithers are small"));
    Assessment big = dither("DITHER by 20.000, 15.000");
    CHECK(Find(big.improve, "Dithers are bigger than needed"));
}

TEST_CASE("dithers that take long to settle are flagged", "[catalogue]")
{
    auto settle = [](size_t frames) {
        return AssessGuiding(GuidingHeader(), Frames(300),
                             { { 100, "DITHER by 4.116, -3.024" }, { 100, "SETTLING STATE CHANGE, Settling started" },
                               { 100 + frames, "SETTLING STATE CHANGE, Settling complete" } });
    };
    Assessment slow = settle(20); // 50 s
    const Finding *f = Find(slow.improve, "Dithers take long to settle");
    REQUIRE(f);
    CHECK(Has(f->body, "50 s"));

    Assessment quick = settle(4); // 10 s
    CHECK_FALSE(Find(quick.improve, "Dithers take long to settle"));
    REQUIRE(Find(quick.good, "Dithers settled"));
    CHECK(Has(Find(quick.good, "Dithers settled")->body, "10 s"));
}

TEST_CASE("a strain-wave mount's noise advice mentions its fast periodic error", "[catalogue]")
{
    auto hdr = With(GuidingHeader(), "Exposure = ", "Exposure = 1000 ms");
    auto frames = Frames(300, 1.2);
    for (size_t i = 0; i < frames.size(); i++)
        if (i % 5 < 3)
            frames[i].raPulse = frames[i].ra > 0 ? -150 : 150;
    Assessment a = AssessGuiding(hdr, frames);
    const Finding *f = Find(a.improve, "RA is correcting noise");
    REQUIRE(f);
    CHECK(Has(f->body, "Strain-wave mounts"));
}

TEST_CASE("a clean synthetic session raises none of the catalogue's checks", "[catalogue]")
{
    Assessment a = AssessGuiding(GuidingHeader(), Frames(300));
    for (const char *title : { "RA is pushed mostly one way", "Calibration was short", "Calibration doesn't match",
                               "Minimum move doesn't match", "Guide speed is slow", "Dec only needs pushing one way",
                               "Bin the guide camera", "Dithers are", "Dithers take long" })
        CHECK_FALSE(Find(a.improve, title));
}
