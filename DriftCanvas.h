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

#ifndef DRIFTCANVAS_INCLUDED
#define DRIFTCANVAS_INCLUDED

// The Drift chart: RA and Dec over time with the straight-line drift
// removed (unless that is turned off), for the selection or the whole
// session. Scroll or drag to pan, and pinch or Command-scroll to zoom.

#include "ChartHost.h"
#include "viewport.h"

#include <wx/window.h>

#include <optional>

class DriftCanvas : public wxWindow, public Chart
{
    ChartHost& m_host;
    Viewport m_view{ 1e6 };
    VerticalView m_vertical;
    unsigned m_shown = 0; // the generation of analysis the view was fitted to
    std::optional<size_t> m_hover;
    std::optional<int> m_dragX;

    wxRect PlotRect() const;
    // fits the view when the analysis changes; null when there is none
    const GARun *Prepare();

    void OnPaint(wxPaintEvent& event);
    void OnWheel(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);

public:
    DriftCanvas(wxWindow *parent, ChartHost& host);

    void ZoomBy(double factor) override;
    void ZoomToFit() override;
    bool CanZoomIn() override { return Prepare() && m_view.CanZoomIn(); }
    bool CanZoomOut() override { return Prepare() && m_view.CanZoomOut(); }
};

#endif
