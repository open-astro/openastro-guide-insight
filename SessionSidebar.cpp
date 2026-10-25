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

#include "SessionSidebar.h"
#include "commands.h"

#include <wx/dc.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/filename.h>
#include <wx/menu.h>

#include <algorithm>

namespace {

class SectionRenderer : public wxDataViewCustomRenderer
{
    const SessionSidebar *m_owner;
    long m_row = 0;

public:
    explicit SectionRenderer(const SessionSidebar *owner)
        : wxDataViewCustomRenderer("long", wxDATAVIEW_CELL_INERT), m_owner(owner) { }

    bool SetValue(const wxVariant& value) override
    {
        m_row = value.GetLong();
        return true;
    }

    bool GetValue(wxVariant& value) const override
    {
        value = m_row;
        return true;
    }

    wxSize GetSize() const override
    {
        const wxWindow *w = GetView();
        return wxSize(w->GetClientSize().x, 2 * w->GetCharHeight() + w->FromDIP(12));
    }

    bool Render(wxRect cell, wxDC *dc, int state) override
    {
        const auto& rows = m_owner->Rows();
        const auto& logs = m_owner->LogRows();
        if (m_row < 0 || m_row >= (long) rows.size())
            return false;

        const SidebarRow& item = rows[m_row];
        const Palette& pal = m_owner->GetPalette();
        const wxWindow *w = GetView();
        bool selected = (state & wxDATAVIEW_CELL_SELECTED) != 0;
        wxColour fg = selected ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT) : pal.text;
        wxColour fg2 = selected ? fg : pal.text2;
        int pad = w->FromDIP(6);
        int dot = w->FromDIP(8);
        int line = cell.height / 2;
        wxRect inner = cell.Deflate(pad, 0);
        int x = inner.x + dot + pad;

        wxFont font = w->GetFont();
        wxFont bold = font.Bold();
        wxFont small = font.Smaller();
        dc->SetFont(bold);
        int y1 = item.section ? cell.y + line - dc->GetCharHeight() + w->FromDIP(3) :
                 cell.y + w->GetCharHeight() / 2 + w->FromDIP(3);

        dc->SetPen(*wxTRANSPARENT_PEN);
        if (!item.section)
        {
            const SidebarLogRow& log = logs[item.log];
            if (log.active)
            {
                dc->SetBrush(pal.accent);
                dc->DrawRectangle(0, cell.y, w->GetClientSize().x, cell.height + w->FromDIP(2));
                fg = *wxWHITE;
                fg2 = fg;
            }
            int arrow = w->FromDIP(7);
            int arrowX = inner.x + arrow / 2;
            int arrowY = y1 + dc->GetCharHeight() / 2;
            dc->SetBrush(selected ? fg : pal.text2);
            if (log.expanded)
            {
                wxPoint points[] = { { arrowX - arrow / 2, arrowY - arrow / 3 },
                                     { arrowX + arrow / 2, arrowY - arrow / 3 },
                                     { arrowX, arrowY + arrow / 3 } };
                dc->DrawPolygon(3, points);
            }
            else
            {
                wxPoint points[] = { { arrowX - arrow / 3, arrowY - arrow / 2 },
                                     { arrowX - arrow / 3, arrowY + arrow / 2 },
                                     { arrowX + arrow / 3, arrowY } };
                dc->DrawPolygon(3, points);
            }

            dc->SetFont(bold);
            dc->SetTextForeground(fg);
            dc->DrawText(log.filename, inner.x + arrow + pad, y1);
            return true;
        }

        const SectionRow& row = item.row;
        dc->SetBrush(selected ? fg : row.calibration ? pal.dec : pal.ra);
        dc->SetTextForeground(fg);
        dc->DrawCircle(inner.x + dot / 2, y1 + dc->GetCharHeight() / 2, dot / 2);
        dc->DrawText(wxString::FromUTF8(row.kind), x, y1);

        if (!row.rms.empty())
        {
            dc->SetFont(font);
            wxString rms = wxString::FromUTF8(row.rms);
            int right = std::min(inner.GetRight(), w->GetClientSize().x - 2 * pad);
            dc->DrawText(rms, right - dc->GetTextExtent(rms).x, y1);
        }

        dc->SetFont(small);
        dc->SetTextForeground(fg2);
        dc->DrawText(wxString::FromUTF8(row.meta), x, cell.y + line + w->FromDIP(1));
        return true;
    }
};

} // namespace

SessionSidebar::SessionSidebar(wxWindow *parent)
    : wxPanel(parent, wxID_ANY), m_palette(CurrentPalette())
{
    auto sizer = new wxBoxSizer(wxVERTICAL);
    auto heading = new wxStaticText(this, wxID_ANY, "Sessions");
    heading->SetFont(GetFont().Bold().Smaller());
    sizer->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    m_day = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_total = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_version = new wxStaticText(this, wxID_ANY, wxEmptyString);
    for (wxStaticText *text : { m_day, m_total, m_version })
        text->SetFont(GetFont().Smaller());
    sizer->Add(m_day, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    sizer->Add(m_total, 0, wxLEFT | wxRIGHT, FromDIP(12));
    sizer->Add(m_version, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    m_list = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxDV_MULTIPLE | wxDV_NO_HEADER | wxBORDER_NONE);
    m_list->SetName("Sessions");
    m_list->AppendColumn(new wxDataViewColumn("Session", new SectionRenderer(this), 0, FromDIP(160),
                                               wxALIGN_LEFT, 0));
    m_list->SetRowHeight(2 * GetCharHeight() + FromDIP(14));
    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    m_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &SessionSidebar::OnSelectionChanged, this);
    m_list->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &SessionSidebar::OnContextMenu, this);
    m_list->Bind(wxEVT_KEY_DOWN, &SessionSidebar::OnKeyDown, this);
    m_list->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        CallAfter(&SessionSidebar::FitColumn);
        event.Skip();
    });
}

void SessionSidebar::FitColumn()
{
    if (m_list->GetColumnCount() > 0)
        m_list->GetColumn(0)->SetWidth(std::max(m_list->GetClientSize().x - FromDIP(2), FromDIP(80)));
}

void SessionSidebar::SetWorkspace(const Workspace& workspace, Units units)
{
    m_updating = true;
    std::vector<wxString> paths;
    for (size_t i = 0; i < workspace.Size(); i++)
        paths.push_back(workspace.Path(i));

    m_collapsed.erase(std::remove_if(m_collapsed.begin(), m_collapsed.end(), [&](const wxString& path) {
        return std::find(paths.begin(), paths.end(), path) == paths.end();
    }), m_collapsed.end());

    std::vector<SidebarRow> rows;
    std::vector<SidebarLogRow> logs;
    for (size_t i = 0; i < workspace.Size(); i++)
    {
        const GuideLog& log = workspace.At(i).Log();
        SidebarLogRow parent;
        parent.filename = wxFileName(workspace.Path(i)).GetFullName();
        parent.active = i == workspace.ActiveIndex();
        parent.expanded = std::find(m_collapsed.begin(), m_collapsed.end(), workspace.Path(i)) == m_collapsed.end();

        logs.push_back(parent);
        rows.push_back({ i, std::nullopt, {} });
        if (parent.expanded)
            for (size_t section = 0; section < log.sections.size(); section++)
                rows.push_back({ i, section, RowFor(log, section, units) });
    }

    bool same = paths == m_paths && rows.size() == m_rows.size();
    if (same)
        for (size_t i = 0; i < rows.size(); i++)
            if (rows[i].log != m_rows[i].log || rows[i].section != m_rows[i].section)
            {
                same = false;
                break;
            }

    m_rows = std::move(rows);
    m_logRows = std::move(logs);
    if (!same)
    {
        m_list->DeleteAllItems();
        for (size_t i = 0; i < m_rows.size(); i++)
        {
            wxVector<wxVariant> values;
            values.push_back((long)i);
            m_list->AppendItem(values);
        }
    }
    else
        m_list->Refresh();

    m_paths = std::move(paths);
    if (workspace.ActiveIndex() < workspace.Size())
    {
        const GuideLog& log = workspace.Active().Log();
        wxDateTime start;
        for (const auto& loc : log.sections)
        {
            const LogSection& section = loc.type == CALIBRATION_SECTION ?
                static_cast<const LogSection&>(log.calibrations[loc.idx]) : log.sessions[loc.idx];
            if (section.starts.IsValid())
            {
                start = section.starts;
                break;
            }
        }
        m_day->SetLabel(start.IsValid() ? wxString::FromUTF8(DayHeading(start)) : wxString());
        m_total->SetLabel(log.sessions.empty() ? wxString() :
                          wxString::FromUTF8("Total guiding " + Duration(TotalGuiding(log))));
        m_version->SetLabel(log.phd_version.empty() ? wxString() : "PHD2 " + wxString::FromUTF8(log.phd_version));
    }
    else
    {
        m_day->SetLabel(wxEmptyString);
        m_total->SetLabel(wxEmptyString);
        m_version->SetLabel(wxEmptyString);
    }
    SetPalette(m_palette);
    Layout();
    FitColumn();

    m_list->UnselectAll();
    if (workspace.ActiveIndex() < workspace.Size())
    {
        const auto sections = workspace.Active().Sections();
        size_t active = workspace.ActiveIndex();
        for (unsigned int row = 0; row < m_rows.size(); row++)
            if (m_rows[row].log == active && m_rows[row].section &&
                std::find(sections.begin(), sections.end(), *m_rows[row].section) != sections.end())
                m_list->SelectRow(row);
    }
    m_updating = false;
}

void SessionSidebar::SetPalette(const Palette& palette)
{
    m_palette = palette;
    for (wxStaticText *text : { m_day, m_total, m_version })
        text->SetForegroundColour(palette.text2);
    m_list->Refresh();
}

void SessionSidebar::OnSelectionChanged(wxDataViewEvent& event)
{
    if (m_updating || !onSelect)
        return;

    int clickedRow = m_list->ItemToRow(event.GetItem());
    if (clickedRow < 0 || clickedRow >= (int)m_rows.size())
        return;
    const SidebarRow& clicked = m_rows[clickedRow];
    if (!clicked.section)
    {
        ToggleCollapsedLog(m_collapsed, m_paths[clicked.log]);
        onSelect(clicked.log, {}, true);
        return;
    }

    std::vector<size_t> selected;
    for (unsigned int row = 0; row < m_rows.size(); row++)
        if (m_list->IsRowSelected(row))
            selected.push_back(row);
    std::vector<size_t> sections = SelectedSections(m_rows, clickedRow, selected);
    if (!sections.empty())
        onSelect(clicked.log, sections, false);
}

void SessionSidebar::OnKeyDown(wxKeyEvent& event)
{
    wxDataViewItemArray selected;
    m_list->GetSelections(selected);
    if (!selected.empty())
    {
        int row = m_list->ItemToRow(selected.front());
        if (row >= 0 && row < (int)m_rows.size() && !m_rows[row].section &&
            (event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT))
        {
            const wxString& path = m_paths[m_rows[row].log];
            bool expand = event.GetKeyCode() == WXK_RIGHT;
            auto collapsed = std::find(m_collapsed.begin(), m_collapsed.end(), path);
            if (expand && collapsed != m_collapsed.end())
                m_collapsed.erase(collapsed);
            else if (!expand && collapsed == m_collapsed.end())
                m_collapsed.push_back(path);
            if (onRefresh)
                onRefresh();
            return;
        }
    }
    event.Skip();
}

void SessionSidebar::OnContextMenu(wxDataViewEvent& event)
{
    int row = m_list->ItemToRow(event.GetItem());
    if (row < 0 || row >= (int)m_rows.size() || m_rows[row].section || !onCloseLog)
        return;
    size_t log = m_rows[row].log;
    wxMenu menu;
    menu.Append(ID_CLOSE_LOG, "Close Log");
    menu.Bind(wxEVT_MENU, [this, log](wxCommandEvent&) { onCloseLog(log); }, ID_CLOSE_LOG);
    PopupMenu(&menu);
}
