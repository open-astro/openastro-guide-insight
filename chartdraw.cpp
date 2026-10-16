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

#include "chartdraw.h"

#include <wx/window.h>

void DrawPanel(wxGraphicsContext& gc, const wxWindow *window, const Palette& pal)
{
    wxSize sz = window->GetClientSize();
    gc.SetPen(wxPen(pal.border));
    gc.SetBrush(pal.chart);
    gc.DrawRoundedRectangle(0.5, 0.5, sz.x - 1., sz.y - 1., window->FromDIP(10));
}

void DrawRings(wxGraphicsContext& gc, const wxWindow *window, const Palette& pal, double cx, double cy,
               const std::vector<double>& radii, const std::vector<wxString>& labels)
{
    gc.SetBrush(*wxTRANSPARENT_BRUSH);
    gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(pal.zero).Style(wxPENSTYLE_SHORT_DASH)));
    for (double r : radii)
        gc.DrawEllipse(cx - r, cy - r, 2 * r, 2 * r);

    gc.SetFont(window->GetFont().Smaller(), pal.text2);
    for (size_t i = 0; i < radii.size() && i < labels.size(); i++)
    {
        double d = radii[i] * 0.7071;
        gc.DrawText(labels[i], cx + d + window->FromDIP(3), cy - d - window->FromDIP(14));
    }
}

wxColour WithAlpha(const wxColour& c, unsigned char alpha)
{
    return wxColour(c.Red(), c.Green(), c.Blue(), alpha);
}
