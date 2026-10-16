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

#ifndef CHARTHOST_INCLUDED
#define CHARTHOST_INCLUDED

// What a chart needs from the window around it.

#include "appsettings.h"
#include "document.h"
#include "theme.h"

#include <wx/event.h>
#include <wx/string.h>

class ChartHost
{
public:
    virtual ~ChartHost() = default;

    virtual Document& Doc() = 0;
    virtual const AppSettings& Settings() const = 0;
    virtual const Palette& Colors() const = 0;

    // Frames were included or excluded.
    virtual void FramesChanged() = 0;
    // The selection changed.
    virtual void SelectionChanged() = 0;
    // The chart was zoomed or scrolled.
    virtual void ViewChanged() = 0;
    // Text for the status line under the chart; empty for the default.
    virtual void ShowStatus(const wxString& text) = 0;
};

// How far a wheel or trackpad scroll moves, in pixels. A trackpad on macOS
// reports pixels; a mouse wheel reports notches.
double WheelPixels(const wxMouseEvent& event);

#endif
