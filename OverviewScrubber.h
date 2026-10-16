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

#ifndef OVERVIEWSCRUBBER_INCLUDED
#define OVERVIEWSCRUBBER_INCLUDED

// The whole session in miniature under the Timeline, with the part the
// chart shows highlighted. Drag the highlight, or click elsewhere to move
// it there.

#include "ChartHost.h"

#include <wx/window.h>

class OverviewScrubber : public wxWindow
{
    ChartHost& m_host;
    bool m_dragging = false;
    double m_grab = 0.; // where in the highlighted range the drag started, as a position

    // x-axis position for an x in this window, over the whole session
    double PositionAtX(double x) const;
    double XAtPosition(double position) const;
    bool Span(double *start, double *end) const;
    void MoveTo(double first);

    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnWheel(wxMouseEvent& event);

public:
    OverviewScrubber(wxWindow *parent, ChartHost& host);
};

#endif
