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

#ifndef THEME_INCLUDED
#define THEME_INCLUDED

// Colours for the parts of the window the app draws itself, in light and
// dark appearances. Native controls take their colours from the system.

#include <wx/colour.h>
#include <wx/dc.h>
#include <wx/window.h>

#include <string>

struct Palette
{
    bool dark;
    wxColour chart;   // chart background
    wxColour grid;
    wxColour zero;    // the zero line
    wxColour text;
    wxColour text2;   // secondary text
    wxColour border;
    wxColour ra, dec;
    wxColour rms;             // the running RMS
    wxColour raFill, decFill; // correction bars
    wxColour excluded;        // shading over excluded frames
    wxColour accent;
    wxColour viewFill;        // the overview's visible range
    wxColour segBg, segSel;   // segmented control
};

// The palette for the system's current appearance. RA and Dec use the
// user's colours when they chose some ("#rrggbb"), else the theme's.
Palette CurrentPalette(const std::string& raColor = std::string(), const std::string& decColor = std::string());

// For windows the app draws itself. Called before the window is created, it
// lets the window paint over its parent's background where the platform
// supports that (macOS and GTK).
void PaintOverParent(wxWindow *window);

// In a paint handler: where painting over the parent is not supported,
// fills the window with its parent's background colour.
void ClearToParent(wxWindow *window, wxDC& dc);

#endif
