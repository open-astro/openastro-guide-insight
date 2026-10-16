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

#include "SegmentedControl.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/renderer.h>

#include <algorithm>
#include <memory>

static const int PAD_X = 12;
static const int HEIGHT_PAD = 9;

SegmentedControl::SegmentedControl(wxWindow *parent, wxWindowID id, const std::vector<wxString>& labels,
                                   const wxString& name)
    : m_labels(labels),
      m_palette(CurrentPalette())
{
    // draw over the parent's background where the platform allows it; the
    // style has to be set before the window is created
    SetBackgroundStyle(IsTransparentBackgroundSupported() ? wxBG_STYLE_TRANSPARENT : wxBG_STYLE_PAINT);
    Create(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS, wxDefaultValidator, name);
    Measure();
    SetInitialSize(DoGetBestSize());

    Bind(wxEVT_PAINT, &SegmentedControl::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &SegmentedControl::OnLeftDown, this);
    Bind(wxEVT_KEY_DOWN, &SegmentedControl::OnKeyDown, this);
    Bind(wxEVT_SET_FOCUS, &SegmentedControl::OnFocus, this);
    Bind(wxEVT_KILL_FOCUS, &SegmentedControl::OnFocus, this);
}

void SegmentedControl::Measure()
{
    // every segment as wide as the widest label, like the native control
    int widest = 0;
    for (const auto& label : m_labels)
        widest = std::max(widest, GetTextExtent(label).x);
    int w = widest + 2 * FromDIP(PAD_X);

    m_edges.clear();
    for (size_t i = 0; i <= m_labels.size(); i++)
        m_edges.push_back((int) i * w);
}

wxSize SegmentedControl::DoGetBestSize() const
{
    return wxSize(m_edges.back(), GetCharHeight() + FromDIP(HEIGHT_PAD));
}

int SegmentedControl::SegmentAt(int x) const
{
    for (size_t i = 0; i + 1 < m_edges.size(); i++)
        if (x < m_edges[i + 1])
            return (int) i;
    return (int) m_labels.size() - 1;
}

void SegmentedControl::SetSelection(int segment)
{
    if (segment < 0 || segment >= (int) m_labels.size() || segment == m_selection)
        return;
    m_selection = segment;
    Refresh();
}

void SegmentedControl::SetPalette(const Palette& palette)
{
    m_palette = palette;
    Refresh();
}

void SegmentedControl::Choose(int segment)
{
    if (segment == m_selection)
        return;
    SetSelection(segment);
    wxCommandEvent event(wxEVT_CHOICE, GetId());
    event.SetEventObject(this);
    event.SetInt(m_selection);
    event.SetString(m_labels[m_selection]);
    ProcessWindowEvent(event);
}

void SegmentedControl::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    if (GetBackgroundStyle() != wxBG_STYLE_TRANSPARENT)
    {
        dc.SetBackground(GetParent()->GetBackgroundColour());
        dc.Clear();
    }

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;

    wxSize sz = GetClientSize();
    double r = FromDIP(6);
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(m_palette.segBg);
    gc->DrawRoundedRectangle(0, 0, sz.x, sz.y, r);

    double inset = FromDIP(2);
    double x0 = m_edges[m_selection] + inset, x1 = m_edges[m_selection + 1] - inset;
    gc->SetBrush(m_palette.segSel);
    gc->SetPen(wxPen(m_palette.dark ? m_palette.segSel : m_palette.border));
    gc->DrawRoundedRectangle(x0, inset, x1 - x0, sz.y - 2 * inset, r - inset);

    // dividers between unselected neighbours
    gc->SetPen(wxPen(m_palette.border));
    for (int i = 1; i < (int) m_labels.size(); i++)
        if (i != m_selection && i != m_selection + 1)
            gc->StrokeLine(m_edges[i], sz.y * 0.25, m_edges[i], sz.y * 0.75);

    for (int i = 0; i < (int) m_labels.size(); i++)
    {
        wxFont font = GetFont();
        if (i == m_selection)
            font.SetWeight(wxFONTWEIGHT_MEDIUM);
        gc->SetFont(font, i == m_selection ? m_palette.text : m_palette.text2);
        double tw, th;
        gc->GetTextExtent(m_labels[i], &tw, &th);
        gc->DrawText(m_labels[i], (m_edges[i] + m_edges[i + 1] - tw) / 2., (sz.y - th) / 2.);
    }

    if (HasFocus())
    {
        wxRect rc(wxPoint((int) x0, (int) inset), wxPoint((int) x1, sz.y - (int) inset));
        wxRendererNative::Get().DrawFocusRect(this, dc, rc.Deflate(FromDIP(2)));
    }
}

void SegmentedControl::OnLeftDown(wxMouseEvent& event)
{
    SetFocus();
    Choose(SegmentAt(event.GetX()));
}

void SegmentedControl::OnKeyDown(wxKeyEvent& event)
{
    switch (event.GetKeyCode())
    {
    case WXK_LEFT:
        Choose(std::max(m_selection - 1, 0));
        break;
    case WXK_RIGHT:
        Choose(std::min(m_selection + 1, (int) m_labels.size() - 1));
        break;
    case WXK_TAB:
        Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward);
        break;
    default:
        event.Skip();
    }
}

void SegmentedControl::OnFocus(wxFocusEvent& event)
{
    Refresh();
    event.Skip();
}
