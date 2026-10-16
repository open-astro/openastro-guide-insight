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

#include "theme.h"

#include <wx/settings.h>

static wxColour Rgba(unsigned char r, unsigned char g, unsigned char b, double a)
{
    return wxColour(r, g, b, (unsigned char) (a * 255. + 0.5));
}

static Palette Light()
{
    Palette p;
    p.dark = false;
    p.chart = wxColour(0xff, 0xff, 0xff);
    p.grid = wxColour(0xe6, 0xe6, 0xeb);
    p.zero = wxColour(0xbd, 0xbd, 0xc4);
    p.text = wxColour(0x1d, 0x1d, 0x1f);
    p.text2 = wxColour(0x63, 0x63, 0x68);
    p.border = wxColour(0xda, 0xda, 0xdf);
    p.ra = wxColour(0x1f, 0x63, 0xd6);
    p.dec = wxColour(0xe0, 0x62, 0x0a);
    p.rms = wxColour(0x1e, 0x8e, 0x4e);
    p.raFill = Rgba(0x1f, 0x63, 0xd6, 0.22);
    p.decFill = Rgba(0xe0, 0x62, 0x0a, 0.24);
    p.excluded = Rgba(0x63, 0x63, 0x68, 0.10);
    p.accent = wxColour(0x0a, 0x6c, 0xff);
    p.viewFill = Rgba(0x0a, 0x6c, 0xff, 0.10);
    p.segBg = wxColour(0xe3, 0xe3, 0xe8);
    p.segSel = wxColour(0xff, 0xff, 0xff);
    p.good = wxColour(0x1e, 0x8e, 0x4e);
    p.fair = wxColour(0xc2, 0x5e, 0x00);
    p.poor = wxColour(0xd7, 0x26, 0x26);
    return p;
}

static Palette Dark()
{
    Palette p;
    p.dark = true;
    p.chart = wxColour(0x14, 0x14, 0x16);
    p.grid = wxColour(0x2a, 0x2a, 0x2f);
    p.zero = wxColour(0x4a, 0x4a, 0x50);
    p.text = wxColour(0xf2, 0xf2, 0xf7);
    p.text2 = wxColour(0xa1, 0xa1, 0xa6);
    p.border = wxColour(0x38, 0x38, 0x3c);
    p.ra = wxColour(0x5b, 0x9b, 0xff);
    p.dec = wxColour(0xff, 0x9a, 0x4d);
    p.rms = wxColour(0x4c, 0xd0, 0x7d);
    p.raFill = Rgba(0x5b, 0x9b, 0xff, 0.30);
    p.decFill = Rgba(0xff, 0x9a, 0x4d, 0.30);
    p.excluded = Rgba(0xff, 0xff, 0xff, 0.07);
    p.accent = wxColour(0x0a, 0x84, 0xff);
    p.viewFill = Rgba(0x0a, 0x84, 0xff, 0.16);
    p.segBg = wxColour(0x38, 0x38, 0x3c);
    p.segSel = wxColour(0x5a, 0x5a, 0x5f);
    p.good = wxColour(0x7e, 0xe2, 0xa3);
    p.fair = wxColour(0xff, 0xb3, 0x6b);
    p.poor = wxColour(0xff, 0x7b, 0x72);
    return p;
}

// A user colour with the alpha of the theme colour it replaces.
static void Override(wxColour& solid, wxColour& fill, const std::string& user)
{
    if (user.empty())
        return;
    wxColour c(user);
    if (!c.IsOk())
        return;
    solid = c;
    fill = wxColour(c.Red(), c.Green(), c.Blue(), fill.Alpha());
}

Palette CurrentPalette(const std::string& raColor, const std::string& decColor)
{
    Palette p = wxSystemSettings::GetAppearance().IsDark() ? Dark() : Light();
    Override(p.ra, p.raFill, raColor);
    Override(p.dec, p.decFill, decColor);
    return p;
}

void PaintOverParent(wxWindow *window)
{
    window->SetBackgroundStyle(window->IsTransparentBackgroundSupported() ? wxBG_STYLE_TRANSPARENT : wxBG_STYLE_PAINT);
}

void ClearToParent(wxWindow *window, wxDC& dc)
{
    if (window->GetBackgroundStyle() == wxBG_STYLE_TRANSPARENT)
        return;
    dc.SetBackground(window->GetParent()->GetBackgroundColour());
    dc.Clear();
}
