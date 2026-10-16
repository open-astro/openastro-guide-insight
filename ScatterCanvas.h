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

#ifndef SCATTERCANVAS_INCLUDED
#define SCATTERCANVAS_INCLUDED

// The Scatter chart: each counted frame's RA against Dec (or dx against
// dy), with labelled scale rings and the elongation ellipse. It shows the
// selection when there is one. Pinch or Command-scroll zooms.

#include "ChartHost.h"

#include <wx/window.h>

class ScatterCanvas : public wxWindow, public Chart
{
    ChartHost& m_host;
    double m_zoom = 1.;

    void OnPaint(wxPaintEvent& event);
    void OnWheel(wxMouseEvent& event);

public:
    ScatterCanvas(wxWindow *parent, ChartHost& host);

    void ZoomBy(double factor) override;
    void ZoomToFit() override;
    bool CanZoomIn() override { return m_zoom < 50.; }
    bool CanZoomOut() override { return m_zoom > 1.; }
};

#endif
