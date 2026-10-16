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

#ifndef CHARTVIEW_INCLUDED
#define CHARTVIEW_INCLUDED

// The middle of the window: a heading, the chart and a status line.

#include "theme.h"

#include <wx/panel.h>

class wxStaticText;

class ChartView : public wxPanel
{
    Palette m_palette;
    wxStaticText *m_title;
    wxStaticText *m_subtitle;
    wxStaticText *m_status;
    wxWindow *m_canvas;
    wxString m_message, m_hint; // shown in the middle of the chart

    void OnPaintCanvas(wxPaintEvent& event);

public:
    explicit ChartView(wxWindow *parent);

    void SetHeading(const wxString& title, const wxString& subtitle);
    void SetStatus(const wxString& status);
    // Text in the middle of the chart, for when there is nothing to draw.
    void SetMessage(const wxString& message, const wxString& hint = wxString());
    void SetPalette(const Palette& palette);

    wxWindow *Canvas() const { return m_canvas; }
};

#endif
