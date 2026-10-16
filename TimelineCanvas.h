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

#ifndef TIMELINECANVAS_INCLUDED
#define TIMELINECANVAS_INCLUDED

// The Timeline chart: RA and Dec (or dx and dy) frame by frame, with guide
// pulses, events, exclusions and the selection.
//
// Scroll to pan, pinch or Command-scroll to zoom about the pointer, and
// Option-scroll to scale the vertical axis. Drag to select frames;
// Command-drag excludes them and Shift-drag includes them. Command-click an
// excluded range to include it again. Option-drag pans vertically.

#include "ChartHost.h"
#include "timeline.h"

#include <wx/graphics.h>
#include <wx/window.h>

#include <optional>

class TimelineCanvas : public wxWindow, public Chart
{
    enum class Drag { None, Select, Exclude, Include, PanVertical };

    ChartHost& m_host;
    std::optional<size_t> m_hover;
    Drag m_drag = Drag::None;
    int m_anchorX = 0, m_dragX = 0, m_lastY = 0;
    bool m_moved = false;

    double XAtIndex(double index);
    double PositionAtX(int x);

    void Paint(wxGraphicsContext& gc, const wxRect& plot);
    void PaintGrid(wxGraphicsContext& gc, const wxRect& plot, double y0);
    void PaintTimeTicks(wxGraphicsContext& gc, const wxRect& plot);
    void PaintEvents(wxGraphicsContext& gc, const wxRect& plot, const IndexRun& frames);
    void PaintRange(wxGraphicsContext& gc, const wxRect& plot, double x0, double x1, const wxColour& fill,
                    const wxColour& edge);

    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnLeave(wxMouseEvent& event);
    void OnWheel(wxMouseEvent& event);
    void OnMagnify(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnCaptureLost(wxMouseCaptureLostEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);

    void EndDrag();
    void SetHover(std::optional<size_t> frame);

public:
    TimelineCanvas(wxWindow *parent, ChartHost& host);

    // Where the data is drawn, inside the axis labels.
    wxRect PlotRect() const;

    // The current session's view, sized to the plot.
    SessionView *View();

    void ZoomBy(double factor) override;
    void ZoomToFit() override;
    bool CanZoomIn() override;
    bool CanZoomOut() override;
    void ZoomToSelection();
    void FitVertical();
    void ClearHover() { SetHover(std::nullopt); }
};

#endif
