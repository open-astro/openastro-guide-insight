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

#include "appsettings.h"

#include <catch2/catch_test_macros.hpp>
#include <wx/fileconf.h>
#include <wx/sstream.h>

namespace {

wxFileConfig Config(const char *text)
{
    wxStringInputStream is(text);
    return wxFileConfig(is);
}

} // namespace

TEST_CASE("With nothing saved, settings have their defaults", "[appsettings]")
{
    wxFileConfig c = Config("");
    AppSettings s = LoadSettings(c);
    CHECK(s.exclusions.settlingByServer);
    CHECK_FALSE(s.exclusions.settlingByDistance);
    CHECK_FALSE(s.exclusions.serverPauses);
    CHECK(s.exclusions.settle.pixels == 1.0);
    CHECK(s.exclusions.settle.seconds == 10.0);
    CHECK(s.raColor.empty());
    CHECK(s.decColor.empty());
    CHECK(s.verticalLock == 0.);
    CHECK(s.display.units == Units::ArcSeconds);
    CHECK(s.view == ChartKind::Timeline);
    CHECK(s.sidebarShown);
    CHECK(s.inspectorShown);
    CHECK_FALSE(s.geometry);
}

TEST_CASE("Settings saved by the old window are read", "[appsettings]")
{
    wxFileConfig c = Config(
        "FileOpenDir=/Users/me/PHD2\n"
        "geometry=1;1280;800;10;20\n"
        "vscale=12.5\n"
        "[settle]\n"
        "excludeByServer=0\n"
        "excludeParametric=1\n"
        "pixels=1.5\n"
        "seconds=20\n"
        "[exclude]\n"
        "serverPauses=1\n"
        "[color]\n"
        "ra=#112233\n"
        "dec=#445566\n");
    AppSettings s = LoadSettings(c);
    CHECK(s.openDir == "/Users/me/PHD2");
    REQUIRE(s.geometry);
    CHECK(s.geometry->maximized);
    CHECK(s.geometry->width == 1280);
    CHECK(s.geometry->y == 20);
    CHECK(s.verticalLock == 12.5);
    CHECK_FALSE(s.exclusions.settlingByServer);
    CHECK(s.exclusions.settlingByDistance);
    CHECK(s.exclusions.serverPauses);
    CHECK(s.exclusions.settle.pixels == 1.5);
    CHECK(s.exclusions.settle.seconds == 20.0);
    CHECK(s.raColor == "#112233");
    CHECK(s.decColor == "#445566");
}

TEST_CASE("The old default colours give way to the theme's", "[appsettings]")
{
    // the old Settings dialog saved its defaults whenever OK was pressed
    wxFileConfig c = Config("[color]\nra=#6464ff\ndec=#FF0000\n");
    AppSettings s = LoadSettings(c);
    CHECK(s.raColor.empty());
    CHECK(s.decColor.empty());
}

TEST_CASE("Settings survive a save and a load", "[appsettings]")
{
    AppSettings s;
    s.exclusions.serverPauses = true;
    s.exclusions.settle.pixels = 0.75;
    s.raColor = "#123456";
    s.verticalLock = 3.25;
    s.openDir = "C:\\Logs";
    s.display.units = Units::Pixels;
    s.display.raDecAxes = false;
    s.display.xaxis = XAxis::Time;
    s.display.events = false;
    s.display.snr = true;
    s.view = ChartKind::Drift;
    s.sidebarShown = false;
    s.inspectorWidth = 333;
    s.geometry = Geometry{ false, 1000, 700, 5, 6 };

    wxFileConfig c = Config("");
    SaveSettings(c, s);
    AppSettings t = LoadSettings(c);

    CHECK(t.exclusions.serverPauses);
    CHECK(t.exclusions.settle.pixels == 0.75);
    CHECK(t.raColor == "#123456");
    CHECK(t.decColor.empty());
    CHECK(t.verticalLock == 3.25);
    CHECK(t.openDir == "C:\\Logs");
    CHECK(t.display.units == Units::Pixels);
    CHECK_FALSE(t.display.raDecAxes);
    CHECK(t.display.xaxis == XAxis::Time);
    CHECK_FALSE(t.display.events);
    CHECK(t.display.snr);
    CHECK(t.view == ChartKind::Drift);
    CHECK_FALSE(t.sidebarShown);
    CHECK(t.inspectorWidth == 333);
    REQUIRE(t.geometry);
    CHECK(t.geometry->width == 1000);
}

TEST_CASE("Analysis options are saved", "[appsettings]")
{
    wxFileConfig c = Config("");
    AppSettings s = LoadSettings(c);
    CHECK(s.analysis.drift == DriftModel::Line);
    CHECK_FALSE(s.analysis.rawRa);

    s.analysis.drift = DriftModel::Curve;
    s.analysis.rawRa = true;
    SaveSettings(c, s);
    AppSettings t = LoadSettings(c);
    CHECK(t.analysis.drift == DriftModel::Curve);
    CHECK(t.analysis.rawRa);
}

TEST_CASE("Choosing the theme colour again forgets the saved one", "[appsettings]")
{
    wxFileConfig c = Config("[color]\nra=#123456\n");
    AppSettings s = LoadSettings(c);
    s.raColor.clear();
    SaveSettings(c, s);
    CHECK_FALSE(c.HasEntry("/color/ra"));
}

TEST_CASE("Saving removes what only the old window used", "[appsettings]")
{
    wxFileConfig c = Config("geometry.splitter1=215\ngeometry.splitter2=330\ngeometry.awin=0;800;600;0;0\n");
    SaveSettings(c, LoadSettings(c));
    CHECK_FALSE(c.HasEntry("/geometry.splitter1"));
    CHECK_FALSE(c.HasEntry("/geometry.splitter2"));
    CHECK_FALSE(c.HasEntry("/geometry.awin"));
}

TEST_CASE("Unknown saved values fall back to the defaults", "[appsettings]")
{
    wxFileConfig c = Config("view=sideways\n[display]\nunits=furlongs\n");
    AppSettings s = LoadSettings(c);
    CHECK(s.view == ChartKind::Timeline);
    CHECK(s.display.units == Units::ArcSeconds);
}

TEST_CASE("Window geometry is read back", "[appsettings]")
{
    auto g = ParseGeometry("0;1280;800;-10;20");
    REQUIRE(g);
    CHECK_FALSE(g->maximized);
    CHECK(g->height == 800);
    CHECK(g->x == -10);
    CHECK(FormatGeometry(*g) == "0;1280;800;-10;20");
}

TEST_CASE("Damaged window geometry is ignored", "[appsettings]")
{
    // the old window read past the end of a short value
    CHECK_FALSE(ParseGeometry(""));
    CHECK_FALSE(ParseGeometry("1;2"));
    CHECK_FALSE(ParseGeometry("0;wide;800;0;0"));
    CHECK_FALSE(ParseGeometry("0;0;0;0;0")); // no size
}
