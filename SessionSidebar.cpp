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

#include <wx/dc.h>
#include <wx/settings.h>
#include <wx/sizer.h>

#include <algorithm>

namespace {

// Draws a section as two lines: its kind and RMS, then its start time and
// length.
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
        // as wide as the column: some platforms size the cell from this
        const wxWindow *w = GetView();
        int width = GetOwner() ? GetOwner()->GetWidth() : w->FromDIP(160);
        return wxSize(width, 2 * w->GetCharHeight() + w->FromDIP(12));
    }

    bool Render(wxRect cell, wxDC *dc, int state) override
    {
        const auto& rows = m_owner->Rows();
        if (m_row < 0 || m_row >= (long) rows.size())
            return false;
        const SectionRow& row = rows[m_row];
        const Palette& pal = m_owner->GetPalette();
        const wxWindow *w = GetView();

        bool selected = (state & wxDATAVIEW_CELL_SELECTED) != 0;
        wxColour fg = selected ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT) : pal.text;
        wxColour fg2 = selected ? fg : pal.text2;

        int pad = w->FromDIP(6);
        int dot = w->FromDIP(8);
        int line = cell.height / 2;
        wxRect inner = cell.Deflate(pad, 0);

        // a dot in the series colour: RA blue for guiding, Dec orange for calibration
        dc->SetPen(*wxTRANSPARENT_PEN);
        dc->SetBrush(selected ? fg : row.calibration ? pal.dec : pal.ra);
        int x = inner.x + dot + pad;

        wxFont font = w->GetFont();
        wxFont bold = font.Bold();
        wxFont small = font.Smaller();

        dc->SetFont(bold);
        dc->SetTextForeground(fg);
        int y1 = cell.y + line - dc->GetCharHeight() + w->FromDIP(3);
        dc->DrawCircle(inner.x + dot / 2, y1 + dc->GetCharHeight() / 2, dot / 2);
        dc->DrawText(wxString::FromUTF8(row.kind), x, y1);

        if (!row.rms.empty())
        {
            dc->SetFont(font);
            wxString rms = wxString::FromUTF8(row.rms);
            // the cell can extend past the list's visible width
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
    m_day = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_day->SetFont(GetFont().Smaller());
    m_total = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_total->SetFont(GetFont().Smaller());
    m_version = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_version->SetFont(GetFont().Smaller());

    sizer->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    sizer->Add(m_day, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    sizer->Add(m_total, 0, wxLEFT | wxRIGHT, FromDIP(12));
    sizer->Add(m_version, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    m_list = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxDV_MULTIPLE | wxDV_NO_HEADER | wxBORDER_NONE);
    m_list->SetName("Sessions");
    auto column = new wxDataViewColumn("Section", new SectionRenderer(this), 0, FromDIP(160),
                                       wxALIGN_LEFT, 0);
    m_list->AppendColumn(column);
    m_list->SetRowHeight(2 * GetCharHeight() + FromDIP(14));
    sizer->Add(m_list, 1, wxEXPAND);

    SetSizer(sizer);

    m_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &SessionSidebar::OnSelectionChanged, this);
    // keep the single column as wide as the list
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

void SessionSidebar::SetLog(const GuideLog& log, const std::vector<size_t>& selected, Units units)
{
    m_updating = true;

    std::vector<SectionRow> rows;
    for (size_t i = 0; i < log.sections.size(); i++)
        rows.push_back(RowFor(log, i, units));

    // the same sections as before: redraw them in place, keeping the
    // list's scroll position and focus
    bool same = rows.size() == m_rows.size() &&
        std::equal(rows.begin(), rows.end(), m_rows.begin(), [](const SectionRow& a, const SectionRow& b) {
            return a.calibration == b.calibration && a.kind == b.kind;
        });
    m_rows = std::move(rows);
    if (same)
        m_list->Refresh();
    else
    {
        m_list->DeleteAllItems();
        for (size_t i = 0; i < m_rows.size(); i++)
        {
            wxVector<wxVariant> values;
            values.push_back(wxVariant((long) i));
            m_list->AppendItem(values);
        }
    }
    // select the rows, if they are not already
    std::vector<size_t> now;
    for (unsigned int r = 0; r < m_rows.size(); r++)
        if (m_list->IsRowSelected(r))
            now.push_back(r);
    if (now != selected)
    {
        m_list->UnselectAll();
        for (size_t r : selected)
            if (r < m_rows.size())
                m_list->SelectRow((unsigned int) r);
    }

    // the day the log starts, from its first section
    wxDateTime start;
    for (const auto& loc : log.sections)
    {
        const LogSection& s = loc.type == CALIBRATION_SECTION ?
            static_cast<const LogSection&>(log.calibrations[loc.idx]) : log.sessions[loc.idx];
        if (s.starts.IsValid())
        {
            start = s.starts;
            break;
        }
    }
    m_day->SetLabel(start.IsValid() ? wxString::FromUTF8(DayHeading(start)) : wxString());
    m_total->SetLabel(log.sessions.empty() ? wxString() :
                      wxString::FromUTF8("Total guiding " + Duration(TotalGuiding(log))));
    m_version->SetLabel(log.phd_version.empty() ? wxString() : "PHD2 " + wxString::FromUTF8(log.phd_version));
    Layout();
    FitColumn();

    m_updating = false;
}

void SessionSidebar::SetPalette(const Palette& palette)
{
    m_palette = palette;
    for (wxStaticText *t : { m_day, m_total, m_version })
        t->SetForegroundColour(palette.text2);
    m_list->Refresh();
}

void SessionSidebar::OnSelectionChanged(wxDataViewEvent&)
{
    if (m_updating)
        return;
    std::vector<size_t> rows;
    for (unsigned int r = 0; r < m_rows.size(); r++)
        if (m_list->IsRowSelected(r))
            rows.push_back(r);
    if (!rows.empty() && onSelect)
        onSelect(rows);
}
