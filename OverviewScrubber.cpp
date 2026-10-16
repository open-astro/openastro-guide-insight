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

#include "OverviewScrubber.h"

#include "timeline.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>

#include <algorithm>
#include <memory>

OverviewScrubber::OverviewScrubber(wxWindow *parent, ChartHost& host)
    : m_host(host)
{
    PaintOverParent(this);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, "Overview");
    SetMinSize(wxSize(-1, FromDIP(48)));
    SetToolTip("The whole session. Drag the highlighted part, or click to move it.");

    Bind(wxEVT_PAINT, &OverviewScrubber::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &OverviewScrubber::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &OverviewScrubber::OnLeftUp, this);
    Bind(wxEVT_MOTION, &OverviewScrubber::OnMotion, this);
    Bind(wxEVT_MOUSEWHEEL, &OverviewScrubber::OnWheel, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { m_dragging = false; });
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { Refresh(); e.Skip(); });
}

bool OverviewScrubber::Span(double *start, double *end) const
{
    const GuideSession *s = m_host.Doc().Session();
    if (!s || s->entries.empty())
        return false;
    AxisSpan(s->entries, m_host.Doc().GetXAxis(), start, end);
    return *end > *start;
}

double OverviewScrubber::XAtPosition(double position) const
{
    double start, end;
    if (!Span(&start, &end))
        return 0.;
    int pad = FromDIP(4);
    return pad + (position - start) / (end - start) * (GetClientSize().x - 2 * pad);
}

double OverviewScrubber::PositionAtX(double x) const
{
    double start, end;
    if (!Span(&start, &end))
        return 0.;
    int pad = FromDIP(4);
    return start + (x - pad) / std::max(GetClientSize().x - 2 * pad, 1) * (end - start);
}

void OverviewScrubber::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    ClearToParent(this, dc);

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;

    const Palette& pal = m_host.Colors();
    wxSize sz = GetClientSize();
    double radius = FromDIP(8);
    gc->SetPen(wxPen(pal.border));
    gc->SetBrush(pal.chart);
    gc->DrawRoundedRectangle(0.5, 0.5, sz.x - 1., sz.y - 1., radius);

    Document& doc = m_host.Doc();
    const GuideSession *s = doc.Session();
    SessionView *v = doc.View();
    double start, end;
    if (!s || !v || !Span(&start, &end))
        return;

    const auto& entries = s->entries;
    XAxis axis = doc.GetXAxis();
    const DisplayOptions& opt = m_host.Settings().display;
    double h = sz.y - FromDIP(8);
    double maxv = std::max(MaxOffset(*s), 1e-6);
    auto yAt = [&](double value) { return FromDIP(4) + h / 2. - value / maxv * (h / 2.); };

    gc->Clip(1, 1, sz.x - 2, sz.y - 2);

    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(pal.excluded);
    for (const IndexRun& run : ExcludedRuns(entries, 0, (unsigned int) entries.size() - 1))
    {
        double x0 = XAtPosition(PositionAtIndex(entries, run.first - 0.5, axis));
        double x1 = XAtPosition(PositionAtIndex(entries, run.last + 0.5, axis));
        gc->DrawRectangle(x0, 0, std::max(x1 - x0, 1.), sz.y);
    }

    // one point per pixel column is plenty for a summary
    size_t stride = std::max<size_t>(1, entries.size() / std::max(sz.x, 1));
    for (Series series : { Series::Ra, Series::Dec })
    {
        if (!(series == Series::Ra ? opt.ra : opt.dec))
            continue;
        wxGraphicsPath path = gc->CreatePath();
        bool started = false;
        for (size_t i = 0; i < entries.size(); i += stride)
        {
            if (!StarWasFound(entries[i].err))
            {
                started = false;
                continue;
            }
            double x = XAtPosition(AxisPosition(entries, i, axis));
            double y = yAt(SeriesValue(entries[i], series, opt.raDecAxes));
            if (started)
                path.AddLineToPoint(x, y);
            else
                path.MoveToPoint(x, y);
            started = true;
        }
        wxColour c = series == Series::Ra ? pal.ra : pal.dec;
        gc->SetPen(wxPen(wxColour(c.Red(), c.Green(), c.Blue(), 180)));
        gc->StrokePath(path);
    }

    // the part the chart shows
    double x0 = XAtPosition(v->horizontal.First()), x1 = XAtPosition(v->horizontal.Last());
    gc->ResetClip();
    gc->SetBrush(pal.viewFill);
    gc->SetPen(wxPen(pal.accent, FromDIP(2)));
    gc->DrawRoundedRectangle(x0, 1, std::max(x1 - x0, (double) FromDIP(6)), sz.y - 2., FromDIP(6));
}

void OverviewScrubber::MoveTo(double first)
{
    SessionView *v = m_host.Doc().View();
    if (!v)
        return;
    double span = v->horizontal.Last() - v->horizontal.First();
    v->horizontal.Show(first, first + span);
    m_host.ViewChanged();
}

void OverviewScrubber::OnLeftDown(wxMouseEvent& event)
{
    SessionView *v = m_host.Doc().View();
    if (!v)
        return;
    double pos = PositionAtX(event.GetX());
    double first = v->horizontal.First(), last = v->horizontal.Last();
    if (pos < first || pos > last)
    {
        // centre the view where the user clicked, then drag from there
        MoveTo(pos - (last - first) / 2.);
        first = v->horizontal.First();
    }
    m_grab = pos - first;
    m_dragging = true;
    if (!HasCapture())
        CaptureMouse();
}

void OverviewScrubber::OnMotion(wxMouseEvent& event)
{
    if (m_dragging)
        MoveTo(PositionAtX(event.GetX()) - m_grab);
}

void OverviewScrubber::OnLeftUp(wxMouseEvent&)
{
    m_dragging = false;
    if (HasCapture())
        ReleaseMouse();
}

void OverviewScrubber::OnWheel(wxMouseEvent& event)
{
    SessionView *v = m_host.Doc().View();
    if (!v)
        return;
    double px = WheelPixels(event);
    v->horizontal.Scroll(event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL ? px : -px);
    m_host.ViewChanged();
}
