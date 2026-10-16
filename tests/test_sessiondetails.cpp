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

#include "sessiondetails.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include <initializer_list>
#include <string>
#include <vector>

namespace {

wxArrayString Header(std::initializer_list<const char *> lines)
{
    wxArrayString hdr;
    for (const char *ln : lines)
        hdr.Add(wxString::FromUTF8(ln));
    return hdr;
}

// "Label: value" per field, for readable failures.
std::vector<std::string> Fields(const wxArrayString& hdr)
{
    std::vector<std::string> v;
    for (const auto& f : SessionDetails(hdr))
        v.push_back(f.label + ": " + f.value);
    return v;
}

// The guiding header from the 2026-10-04 RedCat 51 log.
wxArrayString RedCatGuiding()
{
    return Header({
        "Guiding Begins at 2026-10-04 07:06:47",
        "Equipment Profile = RedCat 51",
        "Dither = both axes, Dither scale = 1.000, Image noise reduction = none, Guide-frame time lapse = 0, Server enabled",
        "Pixel scale = 6.45 arc-sec/px, Binning = 1, Focal length = 120 mm",
        "Search region = 15 px, Star mass tolerance = 50.0%, Single-star mode",
        "Camera = Alpaca Camera [localhost:6800/1] - ZWO ASI120MM Mini (ZWO ASI Camera Driver), full size = 1280 x 960, have dark, dark dur = 1000, no defect map, pixel size = 3.8 um",
        "Exposure = 1000 ms",
        "Mount = Alpaca Mount [localhost:6800/0] - iOptron HAE29C EQ, connected, guiding enabled, xAngle = -114.5, xRate = 0.822, yAngle = -23.6, yRate = 1.045, parity = +/+",
        "Norm rates RA = 7.4\"/s @ dec 0, Dec = 6.7\"/s; ortho.err. = 0.9 deg",
        "X guide algorithm = Hysteresis, Hysteresis = 0.100, Aggression = 0.700, Minimum move = 0.150",
        "Y guide algorithm = Resist Switch, Minimum move = 0.150 Aggression = 70% FastSwitch = enabled",
        "Backlash comp = disabled, pulse = 0 ms",
        "Max RA duration = 2500, Max DEC duration = 2500, DEC guide mode = Auto",
        "RA Guide Speed = 7.5 a-s/s, Dec Guide Speed = 7.5 a-s/s, Cal Dec = 44.5, Last Cal Issue = None, Timestamp = Sun 04 Oct 2026 07:06:47 BST",
        "RA = 21.00 hr, Dec = 44.5 deg, Hour angle = 2.98 hr, Pier side = East, Rotator pos = N/A, Alt = 56.9 deg, Az = 293.1 deg",
        "Lock position = 1160.770, 240.431, Star position = 1166.051, 236.204, HFD = 2.51 px",
    });
}

} // namespace

TEST_CASE("A guiding header becomes labelled fields", "[sessiondetails]")
{
    CHECK(Fields(RedCatGuiding()) == std::vector<std::string>{
        "Profile: RedCat 51",
        "Camera: ZWO ASI120MM Mini",
        "Mount: iOptron HAE29C EQ",
        "Pixel scale: 6.45″/px · 120 mm",
        "Exposure: 1.0 s",
        "RA algorithm: Hysteresis",
        "Dec algorithm: Resist Switch",
        "Pointing: Dec 44.5° · pier East",
    });
}

TEST_CASE("A calibration header has no guide algorithms", "[sessiondetails]")
{
    auto hdr = Header({
        "Calibration Begins at 2026-10-04 06:58:40",
        "Equipment Profile = RedCat 51",
        "Pixel scale = 6.45 arc-sec/px, Binning = 1, Focal length = 120 mm",
        "Camera = Alpaca Camera [localhost:6800/1] - ZWO ASI120MM Mini (ZWO ASI Camera Driver), full size = 1280 x 960, have dark, dark dur = 1000, no defect map, pixel size = 3.8 um",
        "Exposure = 1000 ms",
        "Mount = Alpaca Mount [localhost:6800/0] - iOptron HAE29C EQ, Calibration Step = 750 ms, Calibration Distance = 25 px, Assume orthogonal axes = no",
        "RA = 21.00 hr, Dec = 44.4 deg, Hour angle = 2.85 hr, Pier side = East, Rotator pos = N/A, Alt = 58.3 deg, Az = 292.7 deg",
    });
    CHECK(Fields(hdr) == std::vector<std::string>{
        "Profile: RedCat 51",
        "Camera: ZWO ASI120MM Mini",
        "Mount: iOptron HAE29C EQ",
        "Pixel scale: 6.45″/px · 120 mm",
        "Exposure: 1.0 s",
        "Pointing: Dec 44.4° · pier East",
    });
}

TEST_CASE("A plain device name is shown as it is", "[sessiondetails]")
{
    auto hdr = Header({
        "Camera = ZWO ASI120MM, gain = 50, full size = 1280 x 960, no dark, no defect map, pixel size = 3.8 um",
        "Mount = On Camera, connected, guiding enabled, xAngle = 1.2",
    });
    CHECK(Fields(hdr) == std::vector<std::string>{
        "Camera: ZWO ASI120MM",
        "Mount: On Camera",
    });
}

TEST_CASE("Commas inside brackets stay part of the name", "[sessiondetails]")
{
    auto hdr = Header({ "Camera = INDI Camera [ZWO CCD ASI120MM, Mini], full size = 1280 x 960" });
    CHECK(Fields(hdr) == std::vector<std::string>{ "Camera: INDI Camera [ZWO CCD ASI120MM, Mini]" });
}

TEST_CASE("Guide algorithms belong to the device listed before them", "[sessiondetails]")
{
    auto hdr = Header({
        "AO = SX AO, connected, guiding enabled, xAngle = 0.0",
        "X guide algorithm = Lowpass2, Aggressiveness = 80.000",
        "Y guide algorithm = Lowpass2, Aggressiveness = 80.000",
        "Mount = EQMOD HEQ5/6, connected, guiding enabled, xAngle = 1.2",
        "X guide algorithm = Predictive PEC, Predictive weight = 0.5",
        "Y guide algorithm = Resist Switch, Minimum move = 0.2",
    });
    CHECK(Fields(hdr) == std::vector<std::string>{
        "Mount: EQMOD HEQ5/6",
        "AO: SX AO",
        "RA algorithm: Predictive PEC",
        "Dec algorithm: Resist Switch",
        "AO X algorithm: Lowpass2",
        "AO Y algorithm: Lowpass2",
    });
}

TEST_CASE("Binning other than 1 is shown with the pixel scale", "[sessiondetails]")
{
    auto hdr = Header({ "Pixel scale = 2.10 arc-sec/px, Binning = 2, Focal length = 400 mm" });
    CHECK(Fields(hdr) == std::vector<std::string>{ "Pixel scale: 2.10″/px · bin 2 · 400 mm" });
}

TEST_CASE("Exposures show tenths of a second, or hundredths when needed", "[sessiondetails]")
{
    CHECK(Fields(Header({ "Exposure = 2500 ms" })) == std::vector<std::string>{ "Exposure: 2.5 s" });
    CHECK(Fields(Header({ "Exposure = 1250 ms" })) == std::vector<std::string>{ "Exposure: 1.25 s" });
    CHECK(Fields(Header({ "Exposure = 15000 ms" })) == std::vector<std::string>{ "Exposure: 15.0 s" });
}

TEST_CASE("An unknown pier side is left out", "[sessiondetails]")
{
    auto hdr = Header({ "RA = 5.50 hr, Dec = -12.3 deg, Hour angle = N/A hr, Pier side = Unknown, Rotator pos = N/A" });
    CHECK(Fields(hdr) == std::vector<std::string>{ "Pointing: Dec -12.3°" });
}

TEST_CASE("An empty header has no fields", "[sessiondetails]")
{
    CHECK(SessionDetails(wxArrayString()).empty());
}

namespace {

std::vector<std::string> CalFields(const Calibration& cal)
{
    std::vector<std::string> v;
    for (const auto& f : CalibrationDetails(cal))
        v.push_back(f.label + ": " + f.value);
    return v;
}

} // namespace

TEST_CASE("A calibration's results become labelled fields", "[sessiondetails]")
{
    std::ifstream ifs(TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt");
    GuideLog log;
    REQUIRE(LogParser().Parse(ifs, log));
    REQUIRE(log.calibrations.size() == 1);
    const Calibration& cal = log.calibrations[0];

    int w = 0, e = 0, b = 0, n = 0, s = 0;
    for (const auto& c : cal.entries)
    {
        w += c.direction == WEST;
        e += c.direction == EAST;
        b += c.direction == BACKLASH;
        n += c.direction == NORTH;
        s += c.direction == SOUTH;
    }
    std::string steps = "W " + std::to_string(w) + " \u00b7 E " + std::to_string(e) + " \u00b7 B " +
        std::to_string(b) + " \u00b7 N " + std::to_string(n) + " \u00b7 S " + std::to_string(s);

    CHECK(CalFields(cal) == std::vector<std::string>{
        "RA rate: 0.822 px/s \u00b7 5.30\u2033/s",
        "Dec rate: 1.045 px/s \u00b7 6.74\u2033/s",
        "RA angle: \u2212114.5\u00b0",
        "Dec angle: \u221223.6\u00b0",
        "Orthogonality error: 0.9\u00b0",
        "Step size: 750 ms, 25 px target",
        "Steps: " + steps,
    });
}

TEST_CASE("A calibration that did not finish shows only its steps", "[sessiondetails]")
{
    Calibration cal("2026-10-04 06:58:40");
    cal.hdr.Add("Pixel scale = 2.00 arc-sec/px, Binning = 1, Focal length = 400 mm");
    CalibrationEntry west{ WEST, 1, 0.f, 0.f };
    CalibrationEntry backlash{ BACKLASH, 1, 0.f, 0.f };
    cal.entries = { west, west, backlash };
    CHECK(CalFields(cal) == std::vector<std::string>{ "Steps: W 2 \u00b7 B 1" });
}
