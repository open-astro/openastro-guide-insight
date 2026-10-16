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

#ifndef SEGMENTEDCONTROL_INCLUDED
#define SEGMENTEDCONTROL_INCLUDED

// A row of mutually exclusive buttons, like macOS's segmented control.
// Choosing a segment sends wxEVT_CHOICE with the segment's index as the
// event's int. Left and right arrows move between segments.

#include "theme.h"

#include <wx/control.h>

#include <vector>

class SegmentedControl : public wxControl
{
    friend class SegmentedAccessible;

    std::vector<wxString> m_labels;
    std::vector<int> m_edges; // x where each segment starts, plus the end
    int m_selection = 0;
    Palette m_palette;

    void Measure();
    int SegmentAt(int x) const;
    void Choose(int segment);

    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnFocus(wxFocusEvent& event);

protected:
    wxSize DoGetBestSize() const override;

public:
    SegmentedControl(wxWindow *parent, wxWindowID id, const std::vector<wxString>& labels,
                     const wxString& name);

    int GetSelection() const { return m_selection; }
    void SetSelection(int segment);
    void SetPalette(const Palette& palette);
    bool AcceptsFocus() const override { return true; }
};

#endif
