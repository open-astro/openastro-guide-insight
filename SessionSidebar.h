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
#include "workspace.h"

#include <wx/dataview.h>
#include <wx/panel.h>
#include <wx/stattext.h>

#include <functional>
#include <algorithm>
#include <optional>
#include <wx/string.h>
#include <vector>

struct SidebarRow
{
    size_t log;
    std::optional<size_t> section;
    SectionRow row;
};

struct SidebarLogRow
{
    wxString filename;
    bool active;
    bool expanded;
};

inline bool ToggleCollapsedLog(std::vector<wxString>& collapsed, const wxString& path)
{
    auto it = std::find(collapsed.begin(), collapsed.end(), path);
    if (it == collapsed.end())
    {
        collapsed.push_back(path);
        return false;
    }
    collapsed.erase(it);
    return true;
}

inline std::vector<size_t> SelectedSections(const std::vector<SidebarRow>& rows, size_t clicked,
                                            const std::vector<size_t>& selected)
{
    if (clicked >= rows.size() || !rows[clicked].section)
        return {};

    size_t log = rows[clicked].log;
    std::vector<size_t> sections;
    for (size_t row : selected)
        if (row < rows.size() && rows[row].log == log && rows[row].section)
            sections.push_back(*rows[row].section);
    if (sections.empty())
        sections.push_back(*rows[clicked].section);
    return sections;
}

class SessionSidebar : public wxPanel
{
    Palette m_palette;
    wxStaticText *m_day;
    wxStaticText *m_total;
    wxStaticText *m_version;
    wxDataViewListCtrl *m_list;
    std::vector<wxString> m_paths;
    std::vector<wxString> m_collapsed;
    std::vector<SidebarRow> m_rows;
    std::vector<SidebarLogRow> m_logRows;
    bool m_updating = false;

    void OnSelectionChanged(wxDataViewEvent& event);
    void OnContextMenu(wxDataViewEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void FitColumn();

public:
    explicit SessionSidebar(wxWindow *parent);

    // Called with the log index and its picked sections; parent selection restores that log.
    std::function<void(size_t, const std::vector<size_t>&, bool)> onSelect;
    std::function<void(size_t)> onCloseLog;
    std::function<void()> onRefresh;

    // Shows the open logs as parents and their sections as children.
    void SetWorkspace(const Workspace& workspace, Units units);
    void SetPalette(const Palette& palette);
    void FocusList() { m_list->SetFocus(); }

    const std::vector<SidebarRow>& Rows() const { return m_rows; }
    const std::vector<SidebarLogRow>& LogRows() const { return m_logRows; }
    const Palette& GetPalette() const { return m_palette; }
};

#endif
