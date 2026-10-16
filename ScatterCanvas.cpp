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

#include "ScatterCanvas.h"

#include "chartdraw.h"
#include "labels.h"
#include "scatter.h"
#include "ticks.h"

#include <wx/dcbuffer.h>

#include <algorithm>
#include <math.h>
#include <memory>

ScatterCanvas::ScatterCanvas(wxWindow *parent, ChartHost& host)
    : m_host(host)
{
    PaintOverParent(this);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, "Scatter");
    Bind(wxEVT_PAINT, &ScatterCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { Refresh(); e.Skip(); });
    Bind(wxEVT_MOUSEWHEEL, &ScatterCanvas::OnWheel, this);
    Bind(wxEVT_MAGNIFY, [this](wxMouseEvent& e) { ZoomBy(1. + e.GetMagnification()); });
}

void ScatterCanvas::ZoomBy(double factor)
{
    m_zoom = std::clamp(m_zoom * factor, 1., 50.);
    Refresh();
}

void ScatterCanvas::ZoomToFit()
{
    m_zoom = 1.;
    Refresh();
}

void ScatterCanvas::OnWheel(wxMouseEvent& event)
{
    if (event.CmdDown() && event.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
        ZoomBy(pow(1.1, WheelPixels(event) / 10.));
    else
        event.Skip();
}

void ScatterCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    ClearToParent(this, dc);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;

    const Palette& pal = m_host.Colors();
    DrawPanel(*gc, this, pal);

    Document& doc = m_host.Doc();
    const GuideSession *s = doc.Session();
    if (!s)
        return;
    const DisplayOptions& opt = m_host.Settings().display;
    bool arcsec = opt.units == Units::ArcSeconds;
    double unitsPerPixel = arcsec ? s->pixelScale : 1.;

    std::vector<PlotPoint> points = ScatterPoints(*s, doc.Selection(), opt.raDecAxes);

    wxSize sz = GetClientSize();
    double cx = sz.x / 2., cy = sz.y / 2.;
    double half = std::min(sz.x, sz.y) / 2. - FromDIP(28);
    if (half <= 0.)
        return;

    // fit the farthest point, then zoom
    double radius = std::max(MaxRadius(points), 1e-3);
    double scale = half / (radius * 1.05) * m_zoom; // screen pixels per guide pixel

    // axes through the origin
    gc->SetPen(wxPen(pal.grid));
    gc->StrokeLine(FromDIP(12), cy, sz.x - FromDIP(12), cy);
    gc->StrokeLine(cx, FromDIP(12), cx, sz.y - FromDIP(12));

    // scale rings in the chosen units, out to the panel's edge
    std::vector<double> rings = RingRadii(half / scale * unitsPerPixel, 4);
    std::vector<double> ringPx;
    std::vector<wxString> labels;
    for (double r : rings)
    {
        ringPx.push_back(r / unitsPerPixel * scale);
        int decimals = r >= 1. ? 0 : r >= 0.1 ? 1 : 2;
        labels.push_back(wxString::FromUTF8(Fixed(r, decimals) + (arcsec ? "″" : " px")));
    }
    gc->Clip(1, 1, sz.x - 2, sz.y - 2);
    DrawRings(*gc, this, pal, cx, cy, ringPx, labels);

    // the frames
    double dot = FromDIP(5) / 2.;
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(WithAlpha(pal.ra, pal.dark ? 150 : 130));
    for (const PlotPoint& p : points)
        gc->DrawEllipse(cx + p.x * scale - dot, cy - p.y * scale - dot, 2 * dot, 2 * dot);

    // the elongation ellipse, which the statistics give for RA and Dec
    if (opt.raDecAxes && points.size() >= 2)
    {
        Ellipse e = ElongationEllipse(doc.Stats());
        wxGraphicsMatrix m = gc->CreateMatrix();
        m.Translate(cx + e.cx * scale, cy - e.cy * scale);
        m.Rotate(-e.angle);
        gc->PushState();
        gc->ConcatTransform(m);
        gc->SetPen(wxPen(pal.dec, FromDIP(3) / 2));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        double rx = e.rx * scale, ry = e.ry * scale;
        gc->DrawEllipse(-rx, -ry, 2 * rx, 2 * ry);
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(WithAlpha(pal.dec, 170)).Style(wxPENSTYLE_SHORT_DASH)));
        gc->StrokeLine(-rx * 1.25, 0., rx * 1.25, 0.);
        gc->StrokeLine(0., -ry * 1.25, 0., ry * 1.25);
        gc->PopState();
    }
    gc->ResetClip();

    // axis names
    gc->SetFont(GetFont().Smaller(), pal.text2);
    wxString xName = wxString::FromUTF8(opt.raDecAxes ? "RA →" : "dx →");
    wxString yName = wxString::FromUTF8(opt.raDecAxes ? "Dec ↑" : "dy ↑");
    double tw, th;
    gc->GetTextExtent(xName, &tw, &th);
    gc->DrawText(xName, sz.x - tw - FromDIP(12), cy + FromDIP(4));
    gc->DrawText(yName, cx + FromDIP(6), FromDIP(10));
}
