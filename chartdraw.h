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

#ifndef CHARTDRAW_INCLUDED
#define CHARTDRAW_INCLUDED

// Drawing shared by the charts.

#include "theme.h"

#include <wx/graphics.h>

#include <vector>

// The chart's rounded panel, filling the window.
void DrawPanel(wxGraphicsContext& gc, const wxWindow *window, const Palette& pal);

// Dashed scale rings around (cx, cy), each labelled at its upper right.
void DrawRings(wxGraphicsContext& gc, const wxWindow *window, const Palette& pal, double cx, double cy,
               const std::vector<double>& radii, const std::vector<wxString>& labels);

// A colour with a different opacity.
wxColour WithAlpha(const wxColour& c, unsigned char alpha);

#endif
