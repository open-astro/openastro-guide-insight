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

#include "OverviewScrubber.h"
#include "TimelineCanvas.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <memory>
#include <vector>

ChartView::ChartView(wxWindow *parent, ChartHost& host)
    : wxPanel(parent, wxID_ANY), m_host(host)
{
    int pad = FromDIP(16);
    auto sizer = new wxBoxSizer(wxVERTICAL);

    // heading: title, subtitle, and the legend at the right
    auto heading = new wxBoxSizer(wxHORIZONTAL);
    m_title = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_title->SetFont(GetFont().Bold().Larger());
    m_subtitle = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    m_subtitle->SetFont(GetFont().Smaller());
    m_legend = new wxWindow;
    PaintOverParent(m_legend);
    m_legend->Create(this, wxID_ANY, wxDefaultPosition, wxSize(0, GetCharHeight()), wxBORDER_NONE, "Legend");
    m_legend->Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(m_legend);
        ClearToParent(m_legend, dc);
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc || m_legendRa.empty())
            return;
        const Palette& pal = m_host.Colors();
        gc->SetFont(GetFont().Smaller(), pal.text2);
        double x = 0., h = m_legend->GetClientSize().y;
        for (int i = 0; i < 2; i++)
        {
            const wxString& label = i == 0 ? m_legendRa : m_legendDec;
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(i == 0 ? pal.ra : pal.dec);
            gc->DrawRoundedRectangle(x, h / 2. - FromDIP(2) / 2. - 0.5, FromDIP(14), FromDIP(3), 1.5);
            x += FromDIP(20);
            double tw, th;
            gc->GetTextExtent(label, &tw, &th);
            gc->DrawText(label, x, (h - th) / 2.);
            x += tw + FromDIP(14);
        }
    });
    heading->Add(m_title, 0, wxALIGN_CENTER_VERTICAL);
    // the subtitle gives way to the legend when space is short
    heading->Add(m_subtitle, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(12));
    heading->Add(m_legend, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(12));
    sizer->Add(heading, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
    sizer->AddSpacer(FromDIP(10));

    m_book = new wxSimplebook(this);
    m_message = new wxWindow;
    PaintOverParent(m_message);
    m_message->Create(m_book, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, "Chart");
    m_message->Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(m_message);
        ClearToParent(m_message, dc);
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc)
            return;
        const Palette& pal = m_host.Colors();
        wxSize sz = m_message->GetClientSize();
        gc->SetPen(wxPen(pal.border));
        gc->SetBrush(pal.chart);
        gc->DrawRoundedRectangle(0.5, 0.5, sz.x - 1., sz.y - 1., FromDIP(10));
        if (m_messageText.empty())
            return;
        double tw, th, hw = 0., hh = 0.;
        gc->SetFont(GetFont().Scaled(1.4f).Bold(), pal.text2);
        gc->GetTextExtent(m_messageText, &tw, &th);
        double y = sz.y / 2. - th;
        gc->DrawText(m_messageText, (sz.x - tw) / 2., y);
        if (!m_hint.empty())
        {
            gc->SetFont(GetFont(), pal.text2);
            gc->GetTextExtent(m_hint, &hw, &hh);
            gc->DrawText(m_hint, (sz.x - hw) / 2., y + th + FromDIP(6));
        }
    });
    m_message->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { m_message->Refresh(); e.Skip(); });
    m_timeline = new TimelineCanvas(m_book, host);
    m_book->AddPage(m_message, "Message");
    m_book->AddPage(m_timeline, "Timeline");
    sizer->Add(m_book, 1, wxEXPAND | wxLEFT | wxRIGHT, pad);

    sizer->AddSpacer(FromDIP(10));
    m_overview = new OverviewScrubber(this, host);
    sizer->Add(m_overview, 0, wxEXPAND | wxLEFT | wxRIGHT, pad);

    sizer->AddSpacer(FromDIP(8));
    m_status = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    m_status->SetFont(GetFont().Smaller());
    sizer->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT, pad);
    sizer->AddSpacer(FromDIP(10));

    SetSizer(sizer);
    ApplyPalette();
}

void ChartView::SetHeading(const wxString& title, const wxString& subtitle)
{
    if (m_title->GetLabel() != title)
        m_title->SetLabel(title);
    if (m_subtitle->GetLabel() != subtitle)
        m_subtitle->SetLabel(subtitle);
    Layout();
}

void ChartView::SetLegend(const wxString& ra, const wxString& dec)
{
    m_legendRa = ra;
    m_legendDec = dec;
    // two swatches and their labels
    int width = ra.empty() ? 0 : 2 * FromDIP(34) + GetTextExtent(ra).x + GetTextExtent(dec).x;
    m_legend->SetMinSize(wxSize(width, GetCharHeight()));
    Layout();
    m_legend->Refresh();
}

void ChartView::SetDefaultStatus(const wxString& status)
{
    m_defaultStatus = status;
    m_status->SetLabel(status);
}

void ChartView::ShowStatus(const wxString& status)
{
    m_status->SetLabel(status.empty() ? m_defaultStatus : status);
}

void ChartView::ShowMessage(const wxString& message, const wxString& hint)
{
    m_messageText = message;
    m_hint = hint;
    m_book->ChangeSelection(0);
    m_message->Refresh();
    if (m_overview->IsShown())
    {
        m_overview->Hide();
        Layout();
    }
}

void ChartView::ShowTimeline()
{
    m_book->ChangeSelection(1);
    if (!m_overview->IsShown())
    {
        m_overview->Show();
        Layout();
    }
    RefreshChart();
}

void ChartView::RefreshChart()
{
    m_book->GetCurrentPage()->Refresh();
    m_overview->Refresh();
}

void ChartView::ApplyPalette()
{
    const Palette& pal = m_host.Colors();
    m_subtitle->SetForegroundColour(pal.text2);
    m_status->SetForegroundColour(pal.text2);
    m_legend->Refresh();
    RefreshChart();
}

std::vector<wxWindow *> ChartView::DropTargets() const
{
    return { m_message, m_timeline };
}
