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

#ifndef SESSIONSIDEBAR_INCLUDED
#define SESSIONSIDEBAR_INCLUDED

// The list of the log's sections down the left of the window.

#include "labels.h"
#include "theme.h"

#include <wx/dataview.h>
#include <wx/panel.h>
#include <wx/stattext.h>

#include <functional>
#include <optional>
#include <vector>

class SessionSidebar : public wxPanel
{
    std::vector<SectionRow> m_rows;
    Palette m_palette;
    wxStaticText *m_day;
    wxStaticText *m_total;
    wxStaticText *m_version;
    wxDataViewListCtrl *m_list;
    bool m_updating = false;

    void OnSelectionChanged(wxDataViewEvent& event);
    void FitColumn();

public:
    explicit SessionSidebar(wxWindow *parent);

    // Called with the index into GuideLog::sections when the user picks one.
    std::function<void(size_t)> onSelect;

    // Shows the log's sections, with `current` selected.
    void SetLog(const GuideLog& log, std::optional<size_t> current, Units units);
    void SetPalette(const Palette& palette);
    void FocusList() { m_list->SetFocus(); }

    const std::vector<SectionRow>& Rows() const { return m_rows; }
    const Palette& GetPalette() const { return m_palette; }
};

#endif
