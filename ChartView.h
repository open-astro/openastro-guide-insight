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

// The middle of the window: a heading with the legend, the chart, the
// overview of the whole session, and a status line.

#include "ChartHost.h"

#include <wx/panel.h>

#include <vector>

class OverviewScrubber;
class TimelineCanvas;
class wxSimplebook;
class wxStaticText;

class ChartView : public wxPanel
{
    ChartHost& m_host;
    wxStaticText *m_title;
    wxStaticText *m_subtitle;
    wxWindow *m_legend;
    wxSimplebook *m_book;
    wxWindow *m_message;  // a page with text in the middle, when there is no chart
    TimelineCanvas *m_timeline;
    OverviewScrubber *m_overview;
    wxStaticText *m_status;
    wxString m_messageText, m_hint;
    wxString m_defaultStatus;
    wxString m_legendRa, m_legendDec;

public:
    ChartView(wxWindow *parent, ChartHost& host);

    void SetHeading(const wxString& title, const wxString& subtitle);
    // The legend's labels for the two series; empty hides the legend.
    void SetLegend(const wxString& ra, const wxString& dec);
    // The status line when the pointer is not over a frame.
    void SetDefaultStatus(const wxString& status);
    // The status line for the frame under the pointer; empty for the default.
    void ShowStatus(const wxString& status);

    // Text in the middle, for when there is nothing to chart.
    void ShowMessage(const wxString& message, const wxString& hint = wxString());
    void ShowTimeline();

    void RefreshChart();
    void ApplyPalette();

    TimelineCanvas *Timeline() const { return m_timeline; }
    // Windows that should accept a dropped log file.
    std::vector<wxWindow *> DropTargets() const;
};

#endif
