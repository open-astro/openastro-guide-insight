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

#include "CalibrationCanvas.h"

#include "chartdraw.h"
#include "labels.h"
#include "scatter.h"
#include "ticks.h"

#include <wx/dcbuffer.h>

#include <algorithm>
#include <math.h>
#include <memory>

CalibrationCanvas::CalibrationCanvas(wxWindow *parent, ChartHost& host)
    : m_host(host)
{
    PaintOverParent(this);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, "Calibration");
    SetCursor(wxCursor(wxCURSOR_HAND));

    Bind(wxEVT_PAINT, &CalibrationCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { Refresh(); e.Skip(); });
    Bind(wxEVT_MOUSEWHEEL, &CalibrationCanvas::OnWheel, this);
    Bind(wxEVT_MAGNIFY, [this](wxMouseEvent& e) { ZoomBy(1. + e.GetMagnification()); });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        m_dragFrom = e.GetPosition();
        if (!HasCapture())
            CaptureMouse();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) {
        m_dragFrom.reset();
        if (HasCapture())
            ReleaseMouse();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { m_dragFrom.reset(); });
    Bind(wxEVT_MOTION, &CalibrationCanvas::OnMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) {
        m_hover.reset();
        m_host.ShowStatus(wxEmptyString);
        Refresh();
        e.Skip();
    });
}

CalibrationCanvas::View& CalibrationCanvas::Current()
{
    return m_views[m_host.Doc().CalibrationSection()];
}

void CalibrationCanvas::Geometry(double *scale, double *cx, double *cy)
{
    wxSize sz = GetClientSize();
    const Calibration *cal = m_host.Doc().CalibrationSection();
    double maxv = 0.;
    if (cal)
        for (const PlotPoint& p : CalibrationPoints(*cal))
            maxv = std::max({ maxv, fabs(p.x), fabs(p.y) });
    if (maxv == 0.)
        maxv = 25.; // PHD2's usual calibration distance

    const View& v = Current();
    double half = std::max(std::min(sz.x, sz.y) / 2. - FromDIP(28), 10.);
    *scale = half / (maxv * 1.05) * v.zoom;
    *cx = sz.x / 2. + v.x;
    *cy = sz.y / 2. + v.y;
}

void CalibrationCanvas::ZoomBy(double factor)
{
    View& v = Current();
    double z = std::clamp(v.zoom * factor, 0.25, 40.);
    // keep the middle of the window in place
    v.x *= z / v.zoom;
    v.y *= z / v.zoom;
    v.zoom = z;
    Refresh();
}

void CalibrationCanvas::ZoomToFit()
{
    Current() = View();
    Refresh();
}

void CalibrationCanvas::OnWheel(wxMouseEvent& event)
{
    double px = WheelPixels(event);
    if (event.CmdDown() && event.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
    {
        ZoomBy(pow(1.1, px / 10.));
        return;
    }
    View& v = Current();
    if (event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL)
        v.x -= px;
    else
        v.y += px;
    Refresh();
}

void CalibrationCanvas::OnMotion(wxMouseEvent& event)
{
    if (m_dragFrom)
    {
        View& v = Current();
        v.x += event.GetX() - m_dragFrom->x;
        v.y += event.GetY() - m_dragFrom->y;
        m_dragFrom = event.GetPosition();
        Refresh();
        return;
    }

    // the step under the pointer
    const Calibration *cal = m_host.Doc().CalibrationSection();
    if (!cal)
        return;
    double scale, cx, cy;
    Geometry(&scale, &cx, &cy);
    std::vector<PlotPoint> points = CalibrationPoints(*cal);
    std::optional<size_t> best;
    double bestD = FromDIP(8);
    for (size_t i = 0; i < points.size(); i++)
    {
        double d = hypot(cx + points[i].x * scale - event.GetX(), cy - points[i].y * scale - event.GetY());
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    if (best == m_hover)
        return;
    m_hover = best;
    if (best)
    {
        static const char *const names[] = { "West", "East", "Backlash", "North", "South" };
        const CalibrationEntry& e = cal->entries[*best];
        m_host.ShowStatus(wxString::FromUTF8(std::string(names[e.direction]) + " step " + std::to_string(e.step) +
                                             " · dx " + Fixed(e.dx, 2) + " px · dy " + Fixed(e.dy, 2) + " px"));
    }
    else
        m_host.ShowStatus(wxEmptyString);
    Refresh();
}

void CalibrationCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    ClearToParent(this, dc);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;

    const Palette& pal = m_host.Colors();
    DrawPanel(*gc, this, pal);
    const Calibration *cal = m_host.Doc().CalibrationSection();
    if (!cal)
        return;

    wxSize sz = GetClientSize();
    double scale, cx, cy;
    Geometry(&scale, &cx, &cy);
    gc->Clip(1, 1, sz.x - 2, sz.y - 2);

    // crosshair at the starting point, and rings every few pixels
    gc->SetPen(wxPen(pal.grid));
    gc->StrokeLine(1, cy, sz.x - 1, cy);
    gc->StrokeLine(cx, 1, cx, sz.y - 1);
    double reach = hypot(std::max(cx, sz.x - cx), std::max(cy, sz.y - cy)) / scale;
    std::vector<double> rings = RingRadii(std::min(reach, std::min(sz.x, sz.y) / 2. / scale), 5);
    std::vector<double> ringPx;
    std::vector<wxString> labels;
    for (double r : rings)
    {
        ringPx.push_back(r * scale);
        labels.push_back(wxString::FromUTF8(Fixed(r, r >= 1. ? 0 : 1) + " px"));
    }
    DrawRings(*gc, this, pal, cx, cy, ringPx, labels);

    std::vector<PlotPoint> points = CalibrationPoints(*cal);
    auto at = [&](size_t i) { return wxPoint2DDouble(cx + points[i].x * scale, cy - points[i].y * scale); };

    // the paths west and north
    gc->SetFont(GetFont().Smaller().Bold(), pal.text);
    for (CalDirection d : { WEST, NORTH })
        if (std::optional<IndexRun> span = DirectionSpan(*cal, d))
        {
            wxColour c = d == WEST ? pal.ra : pal.dec;
            gc->SetPen(wxPen(c, FromDIP(2)));
            wxPoint2DDouble a = at(span->first), b = at(span->last);
            gc->StrokeLine(a.m_x, a.m_y, b.m_x, b.m_y);
        }

    // each step, coloured by direction: the return legs fainter
    double dot = FromDIP(7) / 2.;
    std::vector<wxRect> labelled;
    gc->SetFont(GetFont().Smaller(), pal.text2);
    for (size_t i = points.size(); i-- > 0;)
    {
        const CalibrationEntry& e = cal->entries[i];
        wxColour c;
        switch (e.direction)
        {
        case WEST: c = pal.ra; break;
        case EAST: c = WithAlpha(pal.ra, 115); break;
        case NORTH: c = pal.dec; break;
        case SOUTH: c = WithAlpha(pal.dec, 115); break;
        case BACKLASH: c = WithAlpha(pal.text2, 130); break;
        }
        wxPoint2DDouble p = at(i);
        gc->SetPen(m_hover == i ? wxPen(pal.accent, FromDIP(2)) : *wxTRANSPARENT_PEN);
        gc->SetBrush(c);
        gc->DrawEllipse(p.m_x - dot, p.m_y - dot, 2 * dot, 2 * dot);

        // label steps where there is room for the text
        wxString label = wxString::Format("%c%d", "WEBNS"[e.direction], e.step);
        double tw, th;
        gc->GetTextExtent(label, &tw, &th);
        wxRect r((int) (p.m_x + dot + 2), (int) (p.m_y + 2), (int) tw, (int) th);
        if (std::none_of(labelled.begin(), labelled.end(), [&r](const wxRect& o) { return o.Intersects(r); }))
        {
            gc->DrawText(label, r.x, r.y);
            labelled.push_back(r);
        }
    }
    // name each path at its far end, over the step labels
    for (CalDirection d : { WEST, NORTH })
        if (std::optional<IndexRun> span = DirectionSpan(*cal, d))
        {
            wxPoint2DDouble b = at(span->last);
            gc->SetFont(GetFont().Smaller().Bold(), d == WEST ? pal.ra : pal.dec);
            wxString name = d == WEST ? "West" : "North";
            double tw, th;
            gc->GetTextExtent(name, &tw, &th);
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(WithAlpha(pal.chart, 220));
            double x = b.m_x + FromDIP(10), y = b.m_y - th - FromDIP(4);
            gc->DrawRoundedRectangle(x - FromDIP(3), y - 1, tw + FromDIP(6), th + 2, FromDIP(3));
            gc->DrawText(name, x, y);
        }
    gc->ResetClip();
}
