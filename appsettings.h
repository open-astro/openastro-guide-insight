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

#ifndef APPSETTINGS_INCLUDED
#define APPSETTINGS_INCLUDED

// The settings saved between runs, independent of the GUI so they can be
// tested. Keys the old window saved keep their names and meanings.

#include "document.h"
#include "labels.h"
#include "viewport.h"

#include <wx/config.h>

#include <optional>
#include <string>

// A window's size and place, saved as "maximized;width;height;x;y".
struct Geometry
{
    bool maximized = false;
    int width = 0, height = 0, x = 0, y = 0;
};
std::optional<Geometry> ParseGeometry(const wxString& s);
wxString FormatGeometry(const Geometry& g);

// What the chart shows for a guiding session.
enum class ChartKind { Timeline, Scatter, Drift, Frequency };

struct DisplayOptions
{
    Units units = Units::ArcSeconds;
    bool raDecAxes = true; // false shows camera dx and dy
    XAxis xaxis = XAxis::Frames;
    bool ra = true, dec = true;
    bool corrections = true, grid = true, events = true;
    bool limits = false, mass = false, snr = false;
    WhichMount device = MOUNT; // whose corrections to show; not saved
};

struct AppSettings
{
    ExclusionSettings exclusions;
    // "#rrggbb" when the user chose a colour, empty for the theme's
    std::string raColor, decColor;
    double verticalLock = 0.; // locked vertical scale, pixels per arc-second
    wxString openDir;
    DisplayOptions display;
    ChartKind view = ChartKind::Timeline;
    bool sidebarShown = true, inspectorShown = true;
    int sidebarWidth = 230, inspectorWidth = 290;
    std::optional<Geometry> geometry;
};

AppSettings LoadSettings(const wxConfigBase& config);

// Saves the settings, and removes keys only the old window used.
void SaveSettings(wxConfigBase& config, const AppSettings& settings);

#endif
