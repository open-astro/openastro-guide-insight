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

#include <wx/arrstr.h>

#include <algorithm>
#include <initializer_list>
#include <utility>
#include <vector>

std::optional<Geometry> ParseGeometry(const wxString& s)
{
    wxArrayString f = wxSplit(s, ';');
    if (f.size() != 5)
        return std::nullopt;

    long v[5];
    for (size_t i = 0; i < 5; i++)
        if (!f[i].ToLong(&v[i]))
            return std::nullopt;
    if (v[1] <= 0 || v[2] <= 0)
        return std::nullopt;

    return Geometry{ v[0] == 1, (int) v[1], (int) v[2], (int) v[3], (int) v[4] };
}

wxString FormatGeometry(const Geometry& g)
{
    return wxString::Format("%d;%d;%d;%d;%d", g.maximized ? 1 : 0, g.width, g.height, g.x, g.y);
}

namespace {

// An enum saved by name. Unknown names read as the default.
template <typename T>
struct Names
{
    std::vector<std::pair<T, const char *>> names;

    T Read(const wxConfigBase& c, const wxString& key, T dflt) const
    {
        wxString s = c.Read(key, wxEmptyString);
        for (const auto& [value, name] : names)
            if (s == name)
                return value;
        return dflt;
    }

    void Write(wxConfigBase& c, const wxString& key, T value) const
    {
        for (const auto& [v, name] : names)
            if (v == value)
                c.Write(key, wxString(name));
    }
};

const Names<Units> UNITS{ { { Units::ArcSeconds, "arcsec" }, { Units::Pixels, "pixels" } } };
const Names<XAxis> XAXIS{ { { XAxis::Frames, "frames" }, { XAxis::Time, "time" } } };
const Names<ChartKind> VIEWS{ { { ChartKind::Timeline, "timeline" }, { ChartKind::Scatter, "scatter" },
                                { ChartKind::Drift, "drift" }, { ChartKind::Frequency, "frequency" } } };

// A colour the user chose, or empty for the theme's. The old window's
// defaults count as the theme's: its Settings dialog saved them on OK.
std::string ReadColor(const wxConfigBase& c, const wxString& key, const char *oldDefault)
{
    wxString s = c.Read(key, wxEmptyString);
    if (s.IsSameAs(oldDefault, false))
        return std::string();
    return s.ToStdString();
}

void WriteColor(wxConfigBase& c, const wxString& key, const std::string& color)
{
    if (color.empty())
        c.DeleteEntry(key);
    else
        c.Write(key, wxString(color));
}

} // namespace

AppSettings LoadSettings(const wxConfigBase& c)
{
    AppSettings s;

    ExclusionSettings& x = s.exclusions;
    x.settlingByServer = c.ReadBool("/settle/excludeByServer", x.settlingByServer);
    x.settlingByDistance = c.ReadBool("/settle/excludeParametric", x.settlingByDistance);
    x.serverPauses = c.ReadBool("/exclude/serverPauses", x.serverPauses);
    x.settle.pixels = c.ReadDouble("/settle/pixels", x.settle.pixels);
    x.settle.seconds = c.ReadDouble("/settle/seconds", x.settle.seconds);

    s.raColor = ReadColor(c, "/color/ra", "#6464FF");
    s.decColor = ReadColor(c, "/color/dec", "#FF0000");
    s.verticalLock = std::max(c.ReadDouble("/vscale", 0.), 0.);
    s.openDir = c.Read("/FileOpenDir", wxEmptyString);
    s.geometry = ParseGeometry(c.Read("/geometry", wxEmptyString));

    DisplayOptions& d = s.display;
    d.units = UNITS.Read(c, "/display/units", d.units);
    d.raDecAxes = c.Read("/display/axes", "radec") != "camera";
    d.xaxis = XAXIS.Read(c, "/display/xaxis", d.xaxis);
    d.ra = c.ReadBool("/display/ra", d.ra);
    d.dec = c.ReadBool("/display/dec", d.dec);
    d.corrections = c.ReadBool("/display/corrections", d.corrections);
    d.grid = c.ReadBool("/display/grid", d.grid);
    d.events = c.ReadBool("/display/events", d.events);
    d.limits = c.ReadBool("/display/limits", d.limits);
    d.mass = c.ReadBool("/display/mass", d.mass);
    d.snr = c.ReadBool("/display/snr", d.snr);

    s.view = VIEWS.Read(c, "/view", s.view);
    s.sidebarShown = c.ReadBool("/layout/sidebar", s.sidebarShown);
    s.inspectorShown = c.ReadBool("/layout/inspector", s.inspectorShown);
    s.sidebarWidth = (int) c.ReadLong("/layout/sidebarWidth", s.sidebarWidth);
    s.inspectorWidth = (int) c.ReadLong("/layout/inspectorWidth", s.inspectorWidth);
    return s;
}

void SaveSettings(wxConfigBase& c, const AppSettings& s)
{
    const ExclusionSettings& x = s.exclusions;
    c.Write("/settle/excludeByServer", x.settlingByServer);
    c.Write("/settle/excludeParametric", x.settlingByDistance);
    c.Write("/exclude/serverPauses", x.serverPauses);
    c.Write("/settle/pixels", x.settle.pixels);
    c.Write("/settle/seconds", x.settle.seconds);

    WriteColor(c, "/color/ra", s.raColor);
    WriteColor(c, "/color/dec", s.decColor);
    c.Write("/vscale", s.verticalLock);
    c.Write("/FileOpenDir", s.openDir);
    if (s.geometry)
        c.Write("/geometry", FormatGeometry(*s.geometry));

    const DisplayOptions& d = s.display;
    UNITS.Write(c, "/display/units", d.units);
    c.Write("/display/axes", wxString(d.raDecAxes ? "radec" : "camera"));
    XAXIS.Write(c, "/display/xaxis", d.xaxis);
    c.Write("/display/ra", d.ra);
    c.Write("/display/dec", d.dec);
    c.Write("/display/corrections", d.corrections);
    c.Write("/display/grid", d.grid);
    c.Write("/display/events", d.events);
    c.Write("/display/limits", d.limits);
    c.Write("/display/mass", d.mass);
    c.Write("/display/snr", d.snr);

    VIEWS.Write(c, "/view", s.view);
    c.Write("/layout/sidebar", s.sidebarShown);
    c.Write("/layout/inspector", s.inspectorShown);
    c.Write("/layout/sidebarWidth", (long) s.sidebarWidth);
    c.Write("/layout/inspectorWidth", (long) s.inspectorWidth);

    // the old window's splitters and analysis window
    for (const char *key : { "/geometry.splitter1", "/geometry.splitter2", "/geometry.awin" })
        c.DeleteEntry(key);
}
