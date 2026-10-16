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

#include "icons.h"

#include <wx/graphics.h>
#include <wx/image.h>
#include <wx/vector.h>

#include <math.h>
#include <memory>
#include <string.h>

// Draws a 24x24 line icon at the given scale. Drawing it ourselves avoids
// needing wxWidgets' SVG support, which the Linux build leaves out.
static wxBitmap Draw(Icon icon, const wxColour& color, double scale)
{
    int n = (int) lround(20. * scale);
    wxImage image(n, n);
    image.SetAlpha();
    memset(image.GetData(), 0, (size_t) n * n * 3);
    memset(image.GetAlpha(), 0, (size_t) n * n);
    {
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(image));
        if (!gc)
            return wxBitmap(image);
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        gc->Scale(n / 24., n / 24.);
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(color, 1.6).Join(wxJOIN_ROUND).Cap(wxCAP_ROUND)));

        switch (icon)
        {
        case Icon::Open:
        {
            // a folder with a tab at the top left
            wxGraphicsPath p = gc->CreatePath();
            p.MoveToPoint(3.5, 7.);
            p.AddLineToPoint(3.5, 17.5);
            p.AddQuadCurveToPoint(3.5, 19., 5., 19.);
            p.AddLineToPoint(19., 19.);
            p.AddQuadCurveToPoint(20.5, 19., 20.5, 17.5);
            p.AddLineToPoint(20.5, 9.5);
            p.AddQuadCurveToPoint(20.5, 8., 19., 8.);
            p.AddLineToPoint(11.8, 8.);
            p.AddLineToPoint(9.8, 5.5);
            p.AddLineToPoint(5., 5.5);
            p.AddQuadCurveToPoint(3.5, 5.5, 3.5, 7.);
            p.CloseSubpath();
            gc->StrokePath(p);
            break;
        }
        case Icon::Sidebar:
        case Icon::Inspector:
        {
            // a window with a pane down one side
            bool left = icon == Icon::Sidebar;
            gc->DrawRoundedRectangle(3.5, 5., 17., 14., 2.5);
            double x = left ? 9.5 : 14.5;
            gc->StrokeLine(x, 5., x, 19.);
            double x0 = left ? 5.8 : 16.6, x1 = left ? 7.6 : 18.2;
            gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(color, 1.3).Cap(wxCAP_ROUND)));
            for (double y : { 8.5, 11., 13.5 })
                gc->StrokeLine(x0, y, x1, y);
            break;
        }
        }
    }
    return wxBitmap(image);
}

wxBitmapBundle ToolIcon(Icon icon, const wxColour& color)
{
    wxVector<wxBitmap> bitmaps;
    bitmaps.push_back(Draw(icon, color, 1.));
    bitmaps.push_back(Draw(icon, color, 2.));
    return wxBitmapBundle::FromBitmaps(bitmaps);
}
