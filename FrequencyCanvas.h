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

#ifndef FREQUENCYCANVAS_INCLUDED
#define FREQUENCYCANVAS_INCLUDED

// The Frequency chart: the RA periodogram of the selection or the whole
// session on a logarithmic period axis. Pointing at the curve snaps to the
// nearest peak and reads it out; the largest peak is always marked.
// Scroll or drag to pan, and pinch or Command-scroll to zoom.

#include "ChartHost.h"
#include "viewport.h"

#include <wx/window.h>

#include <optional>
#include <vector>

class FrequencyCanvas : public wxWindow, public Chart
{
    ChartHost& m_host;
    Viewport m_view{ 1e6 }; // positions are log10 of the period in seconds
    double m_top = 1.;      // amplitude at the top of the plot, pixels
    unsigned m_shown = 0;
    std::optional<int> m_hoverX; // plot column snapped to a peak
    std::optional<int> m_dragX;

    wxRect PlotRect() const;
    const GARun *Prepare();
    // The curve at each plot column: the tallest data point in the column,
    // or the spline through the data where points are sparser than pixels.
    struct Column
    {
        double period;
        double amplitude;
    };
    std::vector<Column> Columns(const GARun& ga, const wxRect& plot) const;
    double PeriodAt(const GARun& ga, int column) const;

    void OnPaint(wxPaintEvent& event);
    void OnWheel(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);

public:
    FrequencyCanvas(wxWindow *parent, ChartHost& host);

    void ZoomBy(double factor) override;
    void ZoomToFit() override;
    bool CanZoomIn() override { return Prepare() && m_view.CanZoomIn(); }
    bool CanZoomOut() override { return Prepare() && m_view.CanZoomOut(); }
};

#endif
