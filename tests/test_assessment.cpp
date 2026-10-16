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

#include "assessment.h"
#include "document.h"
#include "labels.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <functional>
#include <initializer_list>
#include <math.h>
#include <string>
#include <vector>

using Catch::Approx;

namespace {

wxArrayString Header(std::initializer_list<const char *> lines)
{
    wxArrayString hdr;
    for (const char *ln : lines)
        hdr.Add(wxString::FromUTF8(ln));
    return hdr;
}

GuideEntry Frame(int n, double t, double ra, double dec, int radur = 0, int decdur = 0, int err = 0)
{
    GuideEntry e{};
    e.frame = n;
    e.dt = (float) t;
    e.mount = MOUNT;
    e.included = true;
    e.guiding = true;
    e.dx = e.raraw = (float) ra;
    e.dy = e.decraw = (float) dec;
    e.radur = radur;
    e.decdur = decdur;
    e.snr = 40.f;
    e.err = err;
    return e;
}

// A repeatable uniform number in [-1, 1).
struct Noise
{
    unsigned int state = 12345;
    double operator()()
    {
        state = state * 1103515245u + 12345u;
        return ((state >> 8) & 0xffff) / 32768. - 1.;
    }
};

// A session of n guided frames, one every `cadence` seconds, with the
// RedCat 51 log's header.
GuideSession Session(size_t n, double cadence, const std::function<double(size_t)>& ra,
                     const std::function<double(size_t)>& dec)
{
    GuideSession s(wxString("2026-10-04 07:12:08"));
    s.starts = wxDateTime(4, wxDateTime::Oct, 2026, 7, 12, 8);
    s.pixelScale = 6.45;
    s.mount.isValid = true;
    s.mount.xlim.minMo = 0.15;
    s.mount.ylim.minMo = 0.15;
    s.mount.xlim.maxDur = 2500;
    s.mount.ylim.maxDur = 2500;
    s.hdr = Header({
        "Guiding Begins at 2026-10-04 07:12:08",
        "Pixel scale = 6.45 arc-sec/px, Binning = 1, Focal length = 120 mm",
        "Exposure = 1000 ms",
        "Norm rates RA = 7.4\"/s @ dec 0, Dec = 6.7\"/s; ortho.err. = 0.9 deg",
        "X guide algorithm = Hysteresis, Hysteresis = 0.100, Aggression = 0.500, Minimum move = 0.150",
        "Y guide algorithm = Resist Switch, Minimum move = 0.150 Aggression = 50% FastSwitch = enabled",
        "Max RA duration = 2500, Max DEC duration = 2500, DEC guide mode = Auto",
        "RA Guide Speed = 7.5 a-s/s, Dec Guide Speed = 7.5 a-s/s, Cal Dec = 44.5, Last Cal Issue = None",
        "RA = 21.00 hr, Dec = 44.4 deg, Hour angle = 3.07 hr, Pier side = East, Rotator pos = N/A",
        "Lock position = 1185.020, 270.028, Star position = 1185.020, 270.028, HFD = 3.66 px",
    });
    for (size_t i = 0; i < n; i++)
        s.entries.push_back(Frame((int) i + 1, i * cadence, ra(i), dec(i)));
    if (n)
        s.duration = s.entries.back().dt;
    return s;
}

void AddInfo(GuideSession& s, size_t idx, const char *info)
{
    s.infos.push_back(InfoEntry{ (int) idx, 1, info });
}

GuideLog LogOf(std::vector<GuideSession> sessions)
{
    GuideLog log;
    for (size_t i = 0; i < sessions.size(); i++)
    {
        log.sessions.push_back(sessions[i]);
        log.sections.emplace_back(GUIDING_SECTION, (int) i);
    }
    return log;
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

} // namespace

// ---------------------------------------------------------------------------
// The imaging train

TEST_CASE("the imaging train is read from an OpenAstro guider's header", "[assessment]")
{
    ImagingTrain t = ImagingTrainFrom(Header({
        "Imaging telescope = RedCat 51, Focal length = 250 mm, Aperture = 51 mm, Focal ratio = f/4.9",
        "Imaging camera = ZWO ASI2600MM Pro, Sensor = IMX571, Sensor type = Monochrome, Pixel size = 3.76 x 3.76 um, "
        "Sensor size = 6248 x 4176 px, Binning = 1x1, Pixel scale = 3.10 arc-sec/px, Gain = 100",
    }));
    CHECK(t.telescope == "RedCat 51");
    CHECK(t.camera == "ZWO ASI2600MM Pro");
    REQUIRE(t.focalLength);
    CHECK(*t.focalLength == Approx(250.));
    REQUIRE(t.pixelScale);
    CHECK(*t.pixelScale == Approx(3.10));
}

TEST_CASE("the imaging pixel scale is worked out when the camera line leaves it out", "[assessment]")
{
    ImagingTrain t = ImagingTrainFrom(Header({
        "Imaging telescope = RedCat 51, Focal length = 250 mm",
        "Imaging camera = ZWO ASI2600MM Pro, Pixel size = 3.76 x 3.76 um, Binning = 2x2, Pixel scale = unknown",
    }));
    REQUIRE(t.pixelScale);
    CHECK(*t.pixelScale == Approx(206.265 * 3.76 * 2 / 250).epsilon(0.001));
}

TEST_CASE("an unknown imaging focal length leaves the pixel scale unknown", "[assessment]")
{
    ImagingTrain t = ImagingTrainFrom(Header({
        "Imaging telescope = Mount, Focal length = unknown, Aperture = unknown",
        "Imaging camera = ZWO ASI2600MM Pro, Pixel size = 3.76 x 3.76 um, Binning = 1x1, Pixel scale = unknown",
    }));
    CHECK(t.telescope == "Mount");
    CHECK_FALSE(t.focalLength);
    CHECK_FALSE(t.pixelScale);
}

TEST_CASE("a stock PHD2 header has no imaging train", "[assessment]")
{
    ImagingTrain t = ImagingTrainFrom(Header({
        "Pixel scale = 6.45 arc-sec/px, Binning = 1, Focal length = 120 mm",
        "Camera = Alpaca Camera [localhost:6800/1] - ZWO ASI120MM Mini (ZWO ASI Camera Driver), pixel size = 3.8 um",
    }));
    CHECK(t.telescope.empty());
    CHECK(t.camera.empty());
    CHECK_FALSE(t.focalLength);
    CHECK_FALSE(t.pixelScale);
}

TEST_CASE("an ambiguous imaging camera gives no pixel scale", "[assessment]")
{
    ImagingTrain t = ImagingTrainFrom(Header({
        "Imaging telescope = RedCat 51, Focal length = 250 mm",
        "Imaging camera = ambiguous (2 cameras)",
    }));
    CHECK(t.camera.empty());
    CHECK_FALSE(t.pixelScale);
}

// ---------------------------------------------------------------------------
// The Guiding Assistant's sky floor

namespace {
const char *const GA_RESULT =
    "GA Result - SNR=40.1, Samples=120, Elapsed Time=  120 s, RA HPF-RMS=  0.21 px (  1.35 arc-sec ), "
    "Dec HPF-RMS=  0.19 px (  1.23 arc-sec ), Total HPF-RMS=  0.28 px (  1.83 arc-sec )";
}

TEST_CASE("the sky floor comes from the session's own Guiding Assistant result", "[assessment]")
{
    GuideSession s = Session(20, 2., [](size_t) { return 0.; }, [](size_t) { return 0.; });
    AddInfo(s, 10, GA_RESULT);
    GuideLog log = LogOf({ s });
    auto floor = SkyFloorFor(log, log.sessions[0]);
    REQUIRE(floor);
    CHECK(floor->total == Approx(1.83));
    CHECK(floor->sameSession);
}

TEST_CASE("the sky floor falls back to an earlier session's Guiding Assistant result", "[assessment]")
{
    GuideSession ga = Session(20, 2., [](size_t) { return 0.; }, [](size_t) { return 0.; });
    AddInfo(ga, 10, GA_RESULT);
    GuideSession later = Session(20, 2., [](size_t) { return 0.; }, [](size_t) { return 0.; });
    GuideLog log = LogOf({ ga, later });

    auto floor = SkyFloorFor(log, log.sessions[1]);
    REQUIRE(floor);
    CHECK(floor->total == Approx(1.83));
    CHECK_FALSE(floor->sameSession);

    // a later run says nothing about an earlier session
    GuideLog reversed = LogOf({ later, ga });
    CHECK_FALSE(SkyFloorFor(reversed, reversed.sessions[0]));
}

TEST_CASE("a Guiding Assistant result without the arc-seconds uses the pixel scale", "[assessment]")
{
    GuideSession s = Session(20, 2., [](size_t) { return 0.; }, [](size_t) { return 0.; });
    AddInfo(s, 10, "GA Result - SNR=40.1, Samples=120, Total HPF-RMS=0.30 px");
    GuideLog log = LogOf({ s });
    auto floor = SkyFloorFor(log, log.sessions[0]);
    REQUIRE(floor);
    CHECK(floor->total == Approx(0.30 * 6.45));
}

TEST_CASE("no Guiding Assistant result, no sky floor", "[assessment]")
{
    GuideSession s = Session(20, 2., [](size_t) { return 0.; }, [](size_t) { return 0.; });
    AddInfo(s, 10, "GA Result - Dec Drift Rate=0.3 a-s/min, Dec Peak=1.2 a-s, PA Error=2.1 a-m");
    GuideLog log = LogOf({ s });
    CHECK_FALSE(SkyFloorFor(log, log.sessions[0]));
}

// ---------------------------------------------------------------------------
// Measurements

TEST_CASE("errors that flip sign every frame correlate negatively", "[assessment]")
{
    GuideSession s = Session(200, 2., [](size_t i) { return i % 2 ? 0.3 : -0.3; }, [](size_t) { return 0.; });
    Measurements m = Measure(s, 0, s.entries.size());
    CHECK(m.frames == 200);
    CHECK(m.raLag1 < -0.9);
}

TEST_CASE("random errors barely correlate from frame to frame", "[assessment]")
{
    Noise ra, dec;
    GuideSession s = Session(600, 2., [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    Measurements m = Measure(s, 0, s.entries.size());
    CHECK(fabs(m.raLag1) < 0.15);
    CHECK(fabs(m.decLag1) < 0.15);
}

TEST_CASE("a slow wander correlates strongly", "[assessment]")
{
    GuideSession s = Session(300, 2., [](size_t i) { return 0.5 * sin(i / 20.); }, [](size_t) { return 0.; });
    CHECK(Measure(s, 0, s.entries.size()).raLag1 > 0.9);
}

TEST_CASE("frames that do not count are left out of the measurements", "[assessment]")
{
    GuideSession s = Session(10, 2., [](size_t) { return 0.1; }, [](size_t) { return 0.; });
    s.entries[1].included = false;
    s.entries[2].guiding = false;
    s.entries[3].err = 3; // star lost
    Measurements m = Measure(s, 0, s.entries.size());
    CHECK(m.frames == 7);
    CHECK(m.lost == 1);
}

TEST_CASE("pulses are counted by axis and direction", "[assessment]")
{
    GuideSession s = Session(10, 2., [](size_t) { return 0.1; }, [](size_t) { return 0.1; });
    s.entries[0].radur = 100;
    s.entries[1].radur = -2500; // at the limit
    s.entries[2].decdur = 120;  // north
    s.entries[3].decdur = -90;  // south
    s.entries[4].decdur = -80;  // south
    Measurements m = Measure(s, 0, s.entries.size());
    CHECK(m.raPulsed == Approx(0.2));
    CHECK(m.decPulsed == Approx(0.3));
    CHECK(m.north == 1);
    CHECK(m.south == 2);
    CHECK(m.maxedPulses == 1);
}

TEST_CASE("the measurements read the guiding settings from the header", "[assessment]")
{
    GuideSession s = Session(10, 2., [](size_t) { return 0.; }, [](size_t) { return 0.; });
    Measurements m = Measure(s, 0, s.entries.size());
    CHECK(m.exposure == Approx(1.0));
    CHECK(m.raAggression == Approx(0.5));
    CHECK(m.raMinMove == Approx(0.15));
    CHECK(m.decMinMove == Approx(0.15));
    CHECK(m.hfd == Approx(3.66));
    CHECK(m.orthoError == Approx(0.9));
    CHECK(m.calDec == Approx(44.5));
    CHECK(m.dec == Approx(44.4));
    CHECK(m.medianSnr == Approx(40.));
}

TEST_CASE("a later exposure change in the stretch is the exposure measured", "[assessment]")
{
    GuideSession s = Session(10, 2., [](size_t) { return 0.; }, [](size_t) { return 0.; });
    AddInfo(s, 3, "Exposure = 2000 ms");
    CHECK(Measure(s, 0, s.entries.size()).exposure == Approx(2.0));
    CHECK(Measure(s, 0, 2).exposure == Approx(1.0));
}

TEST_CASE("the worst 5 minutes are found", "[assessment]")
{
    // 30 minutes, rough from minute 12 to 17
    Noise noise;
    auto rough = [](size_t i) { return i * 2. >= 720. && i * 2. < 1020. ? 3. : 1.; };
    GuideSession s = Session(900, 2., [&](size_t i) { return 0.1 * rough(i) * noise(); },
                             [&](size_t i) { return 0.1 * rough(i) * noise(); });
    Measurements m = Measure(s, 0, s.entries.size());
    REQUIRE(m.worst);
    CHECK(m.worstWindow > 2.5 * m.bestWindow);
    // the worst window lies within the rough stretch, give or take a minute
    CHECK(s.entries[m.worst->first].dt >= 660.);
    CHECK(s.entries[m.worst->last].dt <= 1080.);
    CHECK(s.entries[m.worst->last].dt - s.entries[m.worst->first].dt == Approx(300.).margin(2.));
}

TEST_CASE("a stretch under 10 minutes has no worst window", "[assessment]")
{
    Noise noise;
    GuideSession s = Session(250, 2., [&](size_t) { return 0.1 * noise(); }, [&](size_t) { return 0.1 * noise(); });
    CHECK_FALSE(Measure(s, 0, s.entries.size()).worst);
}

TEST_CASE("dithers and dithers that failed to settle are counted", "[assessment]")
{
    GuideSession s = Session(100, 2., [](size_t) { return 0.; }, [](size_t) { return 0.; });
    AddInfo(s, 10, "DITHER by 4.116, -3.024");
    AddInfo(s, 11, "Settling started");
    AddInfo(s, 15, "Settling complete");
    AddInfo(s, 40, "DITHER by 1.0, 2.0");
    AddInfo(s, 41, "Settling started");
    AddInfo(s, 60, "Settling failed");
    AddInfo(s, 90, "DITHER by 1.0, 2.0"); // outside the stretch measured
    Measurements m = Measure(s, 0, 80);
    CHECK(m.dithers == 2);
    CHECK(m.unsettled == 1);
}

TEST_CASE("wind during the worst window is read from the conditions lines", "[assessment]")
{
    Noise noise;
    auto rough = [](size_t i) { return i * 2. >= 720. && i * 2. < 1020. ? 3. : 1.; };
    GuideSession s = Session(900, 2., [&](size_t i) { return 0.1 * rough(i) * noise(); },
                             [&](size_t i) { return 0.1 * rough(i) * noise(); });
    AddInfo(s, 100, "CONDITIONS Seeing = 2.0 arc-sec, Wind speed = 1.0 m/s, Wind gust = 2.0 m/s");
    AddInfo(s, 430, "CONDITIONS Seeing = 2.4 arc-sec, Wind speed = 5.1 m/s, Wind gust = 8.0 m/s");
    Measurements m = Measure(s, 0, s.entries.size());
    REQUIRE(m.worstWind);
    CHECK(*m.worstWind == Approx(8.0));
}

// ---------------------------------------------------------------------------
// The assessment

namespace {

Assessment AssessAll(const GuideSession& s, const GuideStats *stats = nullptr)
{
    GuideLog log = LogOf({ s });
    GuideStats st = stats ? *stats : CalcStats(log.sessions[0], 0, s.entries.size());
    return Assess(log, log.sessions[0], 0, s.entries.size(), st);
}

double TotalRms(const GuideSession& s)
{
    GuideStats st = CalcStats(s, 0, s.entries.size());
    return hypot(st.rms_ra, st.rms_dec) * s.pixelScale;
}

// A session of random errors, with an imaging train whose pixel is `ratio`
// times the total RMS.
GuideSession WithImagingRatio(double ratio)
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    double scale = TotalRms(s) / ratio;
    s.hdr.Add("Imaging telescope = RedCat 51, Focal length = 250 mm");
    s.hdr.Add(wxString::Format("Imaging camera = ZWO ASI2600MM Pro, Pixel size = 3.76 x 3.76 um, Binning = 1x1, "
                               "Pixel scale = %.4f arc-sec/px", scale));
    return s;
}

// Random RA errors with an RA pulse on 60% of frames.
GuideSession NoisyRa(const char *exposure = "Exposure = 1000 ms")
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.05 * dec(); });
    for (size_t i = 0; i < s.entries.size(); i++)
        if (i % 5 < 3)
            s.entries[i].radur = s.entries[i].raraw > 0 ? -150 : 150;
    for (wxString& line : s.hdr)
        if (line.StartsWith("Exposure = "))
            line = exposure;
    return s;
}

} // namespace

TEST_CASE("the verdict compares the total RMS with the imaging pixel", "[assessment]")
{
    CHECK(AssessAll(WithImagingRatio(0.4)).verdict == Verdict::Excellent);
    CHECK(AssessAll(WithImagingRatio(0.67)).verdict == Verdict::Good);
    CHECK(AssessAll(WithImagingRatio(1.2)).verdict == Verdict::Fair);
    CHECK(AssessAll(WithImagingRatio(2.0)).verdict == Verdict::Poor);

    Assessment a = AssessAll(WithImagingRatio(0.67));
    CHECK(a.verdictLabel == "Good");
    CHECK(a.ratio == Approx(0.67).epsilon(0.001));
    CHECK(Has(a.scale, "0.67 of an imaging pixel"));
    CHECK(Has(a.scale, "ZWO ASI2600MM Pro at 250 mm"));
}

TEST_CASE("without the imaging train there is no verdict", "[assessment]")
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    Assessment a = AssessAll(s);
    CHECK(a.verdict == Verdict::Unknown);
    CHECK(a.verdictLabel.empty());
    CHECK(a.ratio == 0.);
    CHECK(Has(a.scale, "telescope and camera"));
    CHECK(a.total == Fixed(TotalRms(s), 2) + "″");
}

TEST_CASE("the explanation puts the total in guide pixels and names the measurement noise", "[assessment]")
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    Assessment a = AssessAll(s);
    CHECK(Has(a.why, Fixed(TotalRms(s) / 6.45, 2) + " of a guide-camera pixel"));
    CHECK(Has(a.why, "SNR 40"));
    CHECK(Has(a.why, "randomly"));
    CHECK(Has(a.why, "no Guiding Assistant run"));
}

TEST_CASE("with a Guiding Assistant result the explanation compares guiding with the sky", "[assessment]")
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    AddInfo(s, 0, GA_RESULT);
    Assessment a = AssessAll(s);
    CHECK(Has(a.why, "Guiding Assistant measured the sky alone at 1.83″"));
    CHECK_FALSE(Has(a.why, "no Guiding Assistant run"));
}

TEST_CASE("RA errors that flip sign every frame mean RA overcorrects", "[assessment]")
{
    GuideSession s = Session(200, 2., [](size_t i) { return i % 2 ? 0.3 : -0.3; }, [](size_t) { return 0.; });
    Assessment a = AssessAll(s);
    const Finding *f = Find(a.improve, "RA overcorrects");
    REQUIRE(f);
    REQUIRE(f->changes.size() == 1);
    CHECK(f->changes[0].setting == "RA aggression");
    CHECK(f->changes[0].from == "50%");
    CHECK(f->changes[0].to == "35%");
}

TEST_CASE("RA errors that carry over mean RA corrections fall behind", "[assessment]")
{
    GuideSession s = Session(300, 2., [](size_t i) { return 0.5 * sin(i / 20.); }, [](size_t) { return 0.; });
    Assessment a = AssessAll(s);
    const Finding *f = Find(a.improve, "RA corrections fall behind");
    REQUIRE(f);
    REQUIRE(f->changes.size() == 1);
    CHECK(f->changes[0].from == "50%");
    CHECK(f->changes[0].to == "65%");
}

TEST_CASE("frequent RA pulses on random errors with short exposures are correcting noise", "[assessment]")
{
    Assessment a = AssessAll(NoisyRa());
    const Finding *f = Find(a.improve, "RA is correcting noise");
    REQUIRE(f);
    REQUIRE_FALSE(f->changes.empty());
    CHECK(f->changes[0].setting == "Exposure");
    CHECK(f->changes[0].from == "1.0 s");
    CHECK(f->changes[0].to == "2–4 s"); // PHD2's Guiding Assistant range
    CHECK(Has(f->evidence, "RA pulses on 60% of frames"));
    // the cost is stated, so the user can weigh it
    CHECK(Has(f->body, "about"));
    CHECK_FALSE(Find(a.improve, "RA overcorrects"));
    CHECK_FALSE(Find(a.improve, "RA corrections fall behind"));
}

TEST_CASE("long exposures are not told to get longer", "[assessment]")
{
    CHECK_FALSE(Find(AssessAll(NoisyRa("Exposure = 3000 ms")).improve, "RA is correcting noise"));
}

TEST_CASE("Dec pulsed both ways with little drift is following noise", "[assessment]")
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.05 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    for (size_t i = 0; i < s.entries.size(); i++)
        if (i % 5 < 2)
            s.entries[i].decdur = s.entries[i].decraw > 0 ? -150 : 150;
    // the pulses here don't move the star, so give the statistics the
    // little drift a real session like this would have
    GuideStats stats = CalcStats(s, 0, s.entries.size());
    stats.drift_dec = 0.005;
    Assessment a = AssessAll(s, &stats);
    const Finding *f = Find(a.improve, "Dec is pushed both ways");
    REQUIRE(f);
    REQUIRE(f->changes.size() == 1);
    CHECK(f->changes[0].setting == "Dec minimum move");
    CHECK(f->changes[0].from == "0.15 px");
    CHECK(f->changes[0].to == "0.25 px");
}

TEST_CASE("Dec pulsed one way is correcting drift, not noise", "[assessment]")
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.05 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    for (size_t i = 0; i < s.entries.size(); i++)
        if (i % 5 < 2)
            s.entries[i].decdur = 150;
    CHECK_FALSE(Find(AssessAll(s).improve, "Dec is pushed both ways"));
}

TEST_CASE("a rough stretch is pointed out with its frames", "[assessment]")
{
    Noise noise;
    auto rough = [](size_t i) { return i * 2. >= 720. && i * 2. < 1020. ? 3. : 1.; };
    GuideSession s = Session(900, 2., [&](size_t i) { return 0.1 * rough(i) * noise(); },
                             [&](size_t i) { return 0.1 * rough(i) * noise(); });
    Assessment a = AssessAll(s);
    const Finding *f = Find(a.improve, "Worse around 07:");
    REQUIRE(f);
    REQUIRE(f->frames);
    CHECK_FALSE(Has(f->body, "wind"));

    AddInfo(s, 430, "CONDITIONS Seeing = 2.4 arc-sec, Wind speed = 5.1 m/s, Wind gust = 8.0 m/s");
    Assessment windy = AssessAll(s);
    f = Find(windy.improve, "Worse around 07:");
    REQUIRE(f);
    CHECK(Has(f->body, "wind"));
    CHECK(Has(f->body, "8.0 m/s"));
}

TEST_CASE("a dither inside the rough stretch is named as a possible cause", "[assessment]")
{
    Noise noise;
    auto rough = [](size_t i) { return i * 2. >= 720. && i * 2. < 1020. ? 3. : 1.; };
    GuideSession s = Session(900, 2., [&](size_t i) { return 0.1 * rough(i) * noise(); },
                             [&](size_t i) { return 0.1 * rough(i) * noise(); });
    AddInfo(s, 420, "DITHER by 4.116, -3.024");
    Measurements m = Measure(s, 0, s.entries.size());
    CHECK(m.worstDithers == 1);
    Assessment a = AssessAll(s);
    const Finding *f = Find(a.improve, "Worse around 07:");
    REQUIRE(f);
    CHECK(Has(f->body, "includes a dither"));
}

TEST_CASE("what went well is listed", "[assessment]")
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    AddInfo(s, 50, "DITHER by 1.0, 2.0");
    AddInfo(s, 60, "Settling complete");
    GuideStats stats = CalcStats(s, 0, s.entries.size());
    stats.paerr = 0.2;
    Assessment a = AssessAll(s, &stats);
    CHECK(Find(a.good, "Star held"));
    CHECK(Find(a.good, "Pulses within limits"));
    CHECK(Find(a.good, "Calibration is current"));
    CHECK(Find(a.good, "Dithers settled"));
    CHECK(Find(a.good, "Polar alignment"));
}

TEST_CASE("problems with the star, pulses, calibration and dithers are flagged", "[assessment]")
{
    Noise ra, dec;
    GuideSession s = Session(300, 2., [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.3 * dec(); });
    for (size_t i = 0; i < 300; i += 20)
        s.entries[i].err = 3; // 15 lost frames, 5%
    for (size_t i = 1; i < 300; i += 50)
        s.entries[i].radur = 2500; // 6 pulses at the limit, 2%
    for (wxString& line : s.hdr)
    {
        if (line.StartsWith("Norm rates"))
            line.Replace("ortho.err. = 0.9 deg", "ortho.err. = 15.2 deg");
        if (line.StartsWith("RA Guide Speed"))
            line.Replace("Cal Dec = 44.5", "Cal Dec = -5.0");
    }
    AddInfo(s, 50, "DITHER by 1.0, 2.0");
    AddInfo(s, 60, "Settling failed");
    AddInfo(s, 100, "DITHER by 1.0, 2.0");
    AddInfo(s, 110, "Settling complete");
    GuideStats stats = CalcStats(s, 0, s.entries.size());
    stats.paerr = 14.;
    Assessment a = AssessAll(s, &stats);
    CHECK(Find(a.improve, "Star lost on 15 frames"));
    CHECK(Find(a.improve, "Pulses hit the 2.5 s limit"));
    CHECK(Find(a.improve, "Calibration is not square"));
    CHECK(Find(a.improve, "Calibrated far from here"));
    CHECK(Find(a.improve, "1 of 2 dithers did not settle"));
    CHECK(Find(a.improve, "Polar alignment is 14′ off"));
    CHECK_FALSE(Find(a.good, "Star held"));
    CHECK_FALSE(Find(a.good, "Dithers settled"));
}


TEST_CASE("the RedCat 51 night is assessed as the guiding it was", "[assessment]")
{
    std::ifstream is(TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt");
    Document doc;
    REQUIRE(doc.Load(is));
    doc.Select(2); // the long session at 07:12
    const GuideSession *s = doc.Session();
    REQUIRE(s);
    Assessment a = Assess(doc.Log(), *s, 0, s->entries.size(), doc.Stats());

    CHECK(a.total == "2.08\u2033");
    CHECK(a.verdict == Verdict::Unknown); // a stock header: no imaging train
    CHECK(Has(a.why, "0.32 of a guide-camera pixel"));
    CHECK(Has(a.why, "randomly"));

    const Finding *noise = Find(a.improve, "RA is correcting noise");
    REQUIRE(noise);
    CHECK(noise->changes[0].from == "0.5 s"); // changed from 1 s early in the session
    CHECK(Find(a.improve, "Frames arrive every 2.5 s"));
    CHECK(Find(a.improve, "Guide star faded"));
    CHECK(Find(a.improve, "Dec may have backlash"));
    CHECK(Find(a.improve, "Dec is pushed both ways"));
    CHECK(Find(a.improve, "Worse around 07:"));
    CHECK(Find(a.good, "Dithers settled"));
    CHECK(Find(a.good, "Polar alignment 0.2\u2032"));
    CHECK(Find(a.good, "Star held"));
}

// ---------------------------------------------------------------------------
// PHD2's Guiding Assistant rules, and the slow loop, fading star, backlash
// and saturation checks

namespace {

GuideSession Quiet(size_t n = 300, double cadence = 2.)
{
    Noise ra, dec;
    return Session(n, cadence, [&](size_t) { return 0.3 * ra(); }, [&](size_t) { return 0.3 * dec(); });
}

void SetHeader(GuideSession& s, const char *prefix, const char *line)
{
    for (wxString& l : s.hdr)
        if (l.StartsWith(prefix))
            l = line;
}

} // namespace

TEST_CASE("polar alignment between 5 and 10 arc-minutes could be improved", "[assessment]")
{
    GuideSession s = Quiet();
    GuideStats stats = CalcStats(s, 0, s.entries.size());
    stats.paerr = 7.;
    Assessment a = AssessAll(s, &stats);
    const Finding *f = Find(a.improve, "Polar alignment is 7′ off");
    REQUIRE(f);
    CHECK(Has(f->body, "could be improved"));

    stats.paerr = 14.;
    Assessment worse = AssessAll(s, &stats);
    f = Find(worse.improve, "Polar alignment is 14′ off");
    REQUIRE(f);
    CHECK(Has(f->body, "drift-align"));

    stats.paerr = 4.;
    CHECK(Find(AssessAll(s, &stats).good, "Polar alignment 4.0′"));
}

TEST_CASE("frames that arrive much slower than the exposure mean a slow guide loop", "[assessment]")
{
    GuideSession s = Quiet(300, 2.5);
    SetHeader(s, "Exposure = ", "Exposure = 500 ms");
    Measurements m = Measure(s, 0, s.entries.size());
    CHECK(m.cadence == Approx(2.5));
    Assessment a = AssessAll(s);
    const Finding *f = Find(a.improve, "Frames arrive every 2.5 s");
    REQUIRE(f);
    CHECK(Has(f->body, "0.5 s"));
    CHECK(Has(f->body, "2.0 s"));

    // a quick loop, or a delay the user asked for, is fine
    GuideSession quick = Quiet(300, 1.3);
    CHECK_FALSE(Find(AssessAll(quick).improve, "Frames arrive every"));
    s.hdr.Add("Dither = both axes, Dither scale = 1.000, Guide-frame time lapse = 2000, Server enabled");
    CHECK_FALSE(Find(AssessAll(s).improve, "Frames arrive every"));
}

TEST_CASE("a guide star that fades through the stretch is flagged", "[assessment]")
{
    GuideSession s = Quiet();
    for (size_t i = 0; i < s.entries.size(); i++)
    {
        s.entries[i].mass = (int) (400000 - 800 * i); // 400k down to 160k
        s.entries[i].snr = (float) (58. - 0.07 * i);
    }
    Assessment a = AssessAll(s);
    const Finding *f = Find(a.improve, "Guide star faded");
    REQUIRE(f);
    CHECK(Has(f->body, "dew"));

    GuideSession steady = Quiet();
    for (GuideEntry& e : steady.entries)
        e.mass = 400000;
    CHECK_FALSE(Find(AssessAll(steady).improve, "Guide star faded"));
}

TEST_CASE("Dec errors that stay put after Dec reverses suggest backlash", "[assessment]")
{
    // Dec pulses switch direction every 10 frames, and the error keeps its
    // sign for 3 frames after each switch
    GuideSession s = Session(300, 2., [](size_t) { return 0.; }, [](size_t i) {
        size_t block = i / 10, k = i % 10;
        double sign = block % 2 ? 1. : -1.;
        return k < 3 ? -sign * 0.3 : sign * 0.3;
    });
    for (size_t i = 0; i < s.entries.size(); i++)
        s.entries[i].decdur = (i / 10) % 2 ? -100 : 100;
    Measurements m = Measure(s, 0, s.entries.size());
    CHECK(m.decReversals >= 20);
    CHECK(m.decStuck == m.decReversals);
    const Finding *f = Find(AssessAll(s).improve, "Dec may have backlash");
    CHECK(f);

    // random Dec errors stay put after a reversal only now and then
    Noise dec;
    GuideSession random = Session(300, 2., [](size_t) { return 0.; }, [&](size_t) { return 0.3 * dec(); });
    for (size_t i = 0; i < random.entries.size(); i++)
        if (i % 3 == 0)
            random.entries[i].decdur = random.entries[i].decraw > 0 ? -100 : 100;
    CHECK_FALSE(Find(AssessAll(random).improve, "Dec may have backlash"));
}

TEST_CASE("likely Dec backlash raises the polar alignment threshold to 10 arc-minutes", "[assessment]")
{
    GuideSession s = Session(300, 2., [](size_t) { return 0.; }, [](size_t i) {
        size_t block = i / 10, k = i % 10;
        double sign = block % 2 ? 1. : -1.;
        return k < 3 ? -sign * 0.3 : sign * 0.3;
    });
    for (size_t i = 0; i < s.entries.size(); i++)
        s.entries[i].decdur = (i / 10) % 2 ? -100 : 100;
    GuideStats stats = CalcStats(s, 0, s.entries.size());
    stats.paerr = 7.;
    CHECK_FALSE(Find(AssessAll(s, &stats).improve, "Polar alignment is"));
}

TEST_CASE("a saturated guide star is flagged", "[assessment]")
{
    GuideSession s = Quiet();
    for (size_t i = 0; i < s.entries.size(); i += 10)
        s.entries[i].err = 1; // 10% saturated, still found
    Assessment a = AssessAll(s);
    CHECK(Find(a.improve, "Guide star saturated"));
    CHECK(Find(a.good, "Star held"));
    CHECK_FALSE(Find(AssessAll(Quiet()).improve, "Guide star saturated"));
}
