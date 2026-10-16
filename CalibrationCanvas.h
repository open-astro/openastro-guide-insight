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

#ifndef CALIBRATIONCANVAS_INCLUDED
#define CALIBRATIONCANVAS_INCLUDED

// The calibration chart: each step's offset from where the calibration
// started, in camera pixels, with the paths west and north. Drag or scroll
// to pan, and pinch or Command-scroll to zoom.

#include "ChartHost.h"

#include <wx/window.h>

#include <map>
#include <optional>

class CalibrationCanvas : public wxWindow, public Chart
{
    struct View
    {
        double zoom = 1.;
        double x = 0., y = 0.; // pan, screen pixels
    };

    ChartHost& m_host;
    std::map<const void *, View> m_views; // per calibration
    std::optional<wxPoint> m_dragFrom;
    std::optional<size_t> m_hover;

    View& Current();
    // screen pixels per camera pixel, and the origin on screen
    void Geometry(double *scale, double *cx, double *cy);

    void OnPaint(wxPaintEvent& event);
    void OnWheel(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);

public:
    CalibrationCanvas(wxWindow *parent, ChartHost& host);

    void ZoomBy(double factor) override;
    void ZoomToFit() override;
    bool CanZoomIn() override { return Current().zoom < 40.; }
    bool CanZoomOut() override { return Current().zoom > 0.25; }
};

#endif
