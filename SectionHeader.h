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

#ifndef SECTIONHEADER_INCLUDED
#define SECTIONHEADER_INCLUDED

// A clickable heading with a disclosure chevron, for the inspector's
// collapsible sections. Clicking it, or pressing Space or Return while it
// has focus, opens or closes the section and calls onToggle.

#include "theme.h"

#include <wx/control.h>

#include <functional>

class SectionHeader : public wxControl
{
    wxString m_title;
    bool m_open = true;
    bool m_small;
    Palette m_palette;

    void OnPaint(wxPaintEvent& event);
    void Toggle();

protected:
    wxSize DoGetBestSize() const override;

public:
    // small: a lighter heading for a disclosure inside a section
    SectionHeader(wxWindow *parent, const wxString& title, bool small = false);

    std::function<void(bool open)> onToggle;

    bool IsOpen() const { return m_open; }
    void SetOpen(bool open);
    void SetTitle(const wxString& title);
    void SetPalette(const Palette& palette);
    bool AcceptsFocus() const override { return true; }
};

#endif
