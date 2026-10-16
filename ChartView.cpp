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

#include "ChartView.h"

#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

ChartView::ChartView(wxWindow *parent)
    : wxPanel(parent, wxID_ANY), m_palette(CurrentPalette())
{
    auto sizer = new wxBoxSizer(wxVERTICAL);

    m_title = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_title->SetFont(GetFont().Bold().Larger());
    m_subtitle = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_subtitle->SetFont(GetFont().Smaller());
    sizer->Add(m_title, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(14));
    sizer->Add(m_subtitle, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));

    m_canvas = new wxWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS,
                            "Chart");
    m_canvas->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_canvas->Bind(wxEVT_PAINT, &ChartView::OnPaintCanvas, this);
    m_canvas->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        m_canvas->Refresh();
        event.Skip();
    });
    sizer->Add(m_canvas, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(14));

    m_status = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxST_ELLIPSIZE_END);
    m_status->SetFont(GetFont().Smaller());
    sizer->Add(m_status, 0, wxEXPAND | wxALL, FromDIP(10));

    SetSizer(sizer);
    SetPalette(m_palette);
}

void ChartView::SetHeading(const wxString& title, const wxString& subtitle)
{
    m_title->SetLabel(title);
    m_subtitle->SetLabel(subtitle);
    Layout();
}

void ChartView::SetStatus(const wxString& status)
{
    m_status->SetLabel(status);
}

void ChartView::SetMessage(const wxString& message, const wxString& hint)
{
    m_message = message;
    m_hint = hint;
    m_canvas->Refresh();
}

void ChartView::SetPalette(const Palette& palette)
{
    m_palette = palette;
    m_subtitle->SetForegroundColour(palette.text2);
    m_status->SetForegroundColour(palette.text2);
    m_canvas->Refresh();
}

void ChartView::OnPaintCanvas(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(m_canvas);
    dc.SetBackground(m_palette.chart);
    dc.Clear();

    wxSize sz = m_canvas->GetClientSize();
    dc.SetPen(wxPen(m_palette.border));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(0, 0, sz.x, sz.y);

    if (m_message.empty())
        return;

    wxFont big = GetFont().Scaled(1.4f).Bold();
    dc.SetFont(big);
    dc.SetTextForeground(m_palette.text2);
    wxSize m = dc.GetTextExtent(m_message);
    int y = sz.y / 2 - m.y;
    dc.DrawText(m_message, (sz.x - m.x) / 2, y);

    if (!m_hint.empty())
    {
        dc.SetFont(GetFont());
        wxSize h = dc.GetTextExtent(m_hint);
        dc.DrawText(m_hint, (sz.x - h.x) / 2, y + m.y + FromDIP(6));
    }
}
