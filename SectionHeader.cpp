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

#include "SectionHeader.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/renderer.h>

#include <memory>

SectionHeader::SectionHeader(wxWindow *parent, const wxString& title, bool small)
    : m_title(title), m_small(small), m_palette(CurrentPalette())
{
    // draw over the parent's background where the platform allows it; the
    // style has to be set before the window is created
    PaintOverParent(this);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS,
           wxDefaultValidator, title);
    SetFont(small ? GetFont().Smaller() : GetFont().Bold());
    SetCursor(wxCursor(wxCURSOR_HAND));
    SetInitialSize(DoGetBestSize());

    Bind(wxEVT_PAINT, &SectionHeader::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
        SetFocus();
        Toggle();
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        switch (event.GetKeyCode())
        {
        case WXK_SPACE:
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
            Toggle();
            break;
        case WXK_TAB:
            Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward);
            break;
        default:
            event.Skip();
        }
    });
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& e) { Refresh(); e.Skip(); });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) { Refresh(); e.Skip(); });
}

wxSize SectionHeader::DoGetBestSize() const
{
    wxSize t = GetTextExtent(m_title);
    return wxSize(t.x + FromDIP(22), t.y + FromDIP(m_small ? 6 : 10));
}

void SectionHeader::SetOpen(bool open)
{
    m_open = open;
    Refresh();
}

void SectionHeader::SetTitle(const wxString& title)
{
    if (title == m_title)
        return;
    m_title = title;
    SetName(title);
    InvalidateBestSize();
    SetInitialSize(DoGetBestSize());
    Refresh();
}

void SectionHeader::SetPalette(const Palette& palette)
{
    m_palette = palette;
    Refresh();
}

void SectionHeader::Toggle()
{
    m_open = !m_open;
    Refresh();
    if (onToggle)
        onToggle(m_open);
}

void SectionHeader::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    ClearToParent(this, dc);

    wxSize sz = GetClientSize();
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;

    // a chevron pointing right when closed, down when open
    double cx = FromDIP(6), cy = sz.y / 2., r = FromDIP(m_small ? 6 : 7) / 2.;
    gc->SetPen(wxPen(m_palette.text2, FromDIP(2) * 0.8));
    wxGraphicsPath path = gc->CreatePath();
    if (m_open)
    {
        path.MoveToPoint(cx - r, cy - r / 2);
        path.AddLineToPoint(cx, cy + r / 2);
        path.AddLineToPoint(cx + r, cy - r / 2);
    }
    else
    {
        path.MoveToPoint(cx - r / 2, cy - r);
        path.AddLineToPoint(cx + r / 2, cy);
        path.AddLineToPoint(cx - r / 2, cy + r);
    }
    gc->StrokePath(path);

    gc->SetFont(GetFont(), m_small ? m_palette.text2 : m_palette.text);
    double tw, th;
    gc->GetTextExtent(m_title, &tw, &th);
    double x = FromDIP(18);
    gc->DrawText(m_title, x, (sz.y - th) / 2.);

    if (HasFocus())
        wxRendererNative::Get().DrawFocusRect(this, dc, wxRect(0, 0, (int) (x + tw) + FromDIP(4), sz.y));
}
