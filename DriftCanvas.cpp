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

#include "DriftCanvas.h"

#include "chartdraw.h"
#include "labels.h"
#include "ticks.h"

#include <wx/dcbuffer.h>

#include <algorithm>
#include <math.h>
#include <memory>

DriftCanvas::DriftCanvas(wxWindow *parent, ChartHost& host)
    : m_host(host)
{
    PaintOverParent(this);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, "Drift");

    Bind(wxEVT_PAINT, &DriftCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        m_view.SetWidth(PlotRect().width);
        Refresh();
        e.Skip();
    });
    Bind(wxEVT_MOUSEWHEEL, &DriftCanvas::OnWheel, this);
    Bind(wxEVT_MAGNIFY, [this](wxMouseEvent& e) {
        if (Prepare())
        {
            m_view.ZoomAbout(1. + e.GetMagnification(), e.GetX() - PlotRect().x);
            Refresh();
        }
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        m_dragX = e.GetX();
        if (!HasCapture())
            CaptureMouse();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) {
        m_dragX.reset();
        if (HasCapture())
            ReleaseMouse();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { m_dragX.reset(); });
    Bind(wxEVT_MOTION, &DriftCanvas::OnMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) {
        m_hover.reset();
        m_host.ShowStatus(wxEmptyString);
        Refresh();
        e.Skip();
    });
}

wxRect DriftCanvas::PlotRect() const
{
    wxSize sz = GetClientSize();
    int left = FromDIP(52), right = FromDIP(14), top = FromDIP(26), bottom = FromDIP(12);
    return wxRect(left, top, std::max(sz.x - left - right, 1), std::max(sz.y - top - bottom, 1));
}

const GARun *DriftCanvas::Prepare()
{
    const GARun *ga = m_host.Analysis();
    if (m_host.AnalysisGeneration() != m_shown)
    {
        m_shown = m_host.AnalysisGeneration();
        m_hover.reset();
        if (ga && ga->len >= 2)
        {
            wxRect plot = PlotRect();
            m_view.SetWidth(plot.width);
            m_view.SetContent(ga->t[0], ga->t[ga->len - 1]);
            double maxv = 0.;
            for (size_t i = 0; i < ga->len; i++)
                maxv = std::max({ maxv, fabs(ga->rac[i]), fabs(ga->decc[i]) });
            m_vertical.Fit(maxv, plot.height);
        }
    }
    return ga && ga->len >= 2 ? ga : nullptr;
}

void DriftCanvas::ZoomBy(double factor)
{
    if (Prepare())
    {
        m_view.ZoomAbout(factor, PlotRect().width / 2.);
        Refresh();
    }
}

void DriftCanvas::ZoomToFit()
{
    m_shown = 0; // Prepare fits both axes to the analysis again
    Prepare();
    Refresh();
}

void DriftCanvas::OnWheel(wxMouseEvent& event)
{
    if (!Prepare())
        return;
    double px = WheelPixels(event);
    bool horizontal = event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL;
    if (event.CmdDown() && !horizontal)
        m_view.ZoomAbout(pow(1.1, px / 10.), event.GetX() - PlotRect().x);
    else if (event.AltDown() && !horizontal)
        m_vertical.Zoom(pow(1.1, px / 10.));
    else
        m_view.Scroll(horizontal ? px : -px);
    m_hover.reset();
    Refresh();
}

void DriftCanvas::OnMotion(wxMouseEvent& event)
{
    const GARun *ga = Prepare();
    if (!ga)
        return;
    if (m_dragX)
    {
        m_view.Scroll(*m_dragX - event.GetX());
        m_dragX = event.GetX();
        Refresh();
        return;
    }

    wxRect plot = PlotRect();
    std::optional<size_t> hover;
    if (plot.Contains(event.GetPosition()))
    {
        double t = m_view.PositionAt(event.GetX() - plot.x);
        size_t i = (size_t) (std::lower_bound(ga->t, ga->t + ga->len, t) - ga->t);
        if (i > 0 && (i == ga->len || t - ga->t[i - 1] < ga->t[i] - t))
            i--;
        hover = std::min(i, ga->len - 1);
    }
    if (hover == m_hover)
        return;
    m_hover = hover;
    if (hover)
    {
        Units units = m_host.Settings().display.units;
        wxDateTime when = ga->starts + wxTimeSpan::Milliseconds(llround(ga->t[*hover] * 1000.));
        m_host.ShowStatus(wxString::FromUTF8(when.Format("%H:%M:%S").ToStdString() +
                                             " · RA " + Distance(ga->rac[*hover], ga->pixscale, units) +
                                             " · Dec " + Distance(ga->decc[*hover], ga->pixscale, units)));
    }
    else
        m_host.ShowStatus(wxEmptyString);
    Refresh();
}

void DriftCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    ClearToParent(this, dc);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;
    const Palette& pal = m_host.Colors();
    DrawPanel(*gc, this, pal);

    const GARun *ga = Prepare();
    if (!ga)
        return;

    const DisplayOptions& opt = m_host.Settings().display;
    wxRect plot = PlotRect();
    double h = plot.height;
    auto xAt = [&](double t) { return plot.x + m_view.XAt(t); };
    auto yAt = [&](double v) { return plot.y + m_vertical.YAt(v, h); };
    double y0 = yAt(0.);

    // value grid in the chosen units
    bool arcsec = opt.units == Units::ArcSeconds;
    double unitsPerPixel = arcsec ? ga->pixscale : 1.;
    double pxPerUnit = m_vertical.Scale() / unitsPerPixel;
    gc->SetFont(GetFont().Smaller(), pal.text2);
    if (opt.grid && pxPerUnit > 0.)
    {
        double step = NiceStep(h / 12. / pxPerUnit);
        for (double v : StepsBetween(-(plot.GetBottom() - y0) / pxPerUnit, (y0 - plot.y) / pxPerUnit, step))
        {
            double y = y0 - v * pxPerUnit;
            gc->SetPen(wxPen(pal.grid));
            if (fabs(v) > step / 2.)
                gc->StrokeLine(plot.x, y, plot.GetRight() + 1, y);
            wxString label = wxString::FromUTF8(Fixed(v, step >= 1. ? 0 : step >= 0.1 ? 1 : 2) + (arcsec ? "″" : ""));
            double tw, th;
            gc->GetTextExtent(label, &tw, &th);
            gc->DrawText(label, plot.x - tw - FromDIP(6), y - th / 2.);
        }

        // clock times
        double t0 = m_view.First(), t1 = m_view.Last();
        if (t1 > t0)
        {
            double tstep = TimeStep(FromDIP(80) * (t1 - t0) / plot.width);
            gc->SetPen(wxPen(pal.grid));
            for (const TimeTick& tick : TimeTicks(ga->starts, t0, t1, tstep))
            {
                double x = xAt(tick.t);
                gc->StrokeLine(x, plot.y - FromDIP(6), x, plot.GetBottom() + 1);
                gc->DrawText(wxString::FromUTF8(tick.label), x + FromDIP(3), plot.y - FromDIP(22));
            }
        }
    }

    gc->Clip(plot.x, plot.y, plot.width, plot.height);
    gc->SetPen(wxPen(pal.zero));
    gc->StrokeLine(plot.x, y0, plot.GetRight() + 1, y0);

    double width = FromDIP(16) / 10.;
    for (int series = 0; series < 2; series++)
    {
        if (!(series == 0 ? opt.ra : opt.dec))
            continue;
        const double *values = series == 0 ? ga->rac : ga->decc;
        wxGraphicsPath path = gc->CreatePath();
        path.MoveToPoint(xAt(ga->t[0]), yAt(values[0]));
        for (size_t i = 1; i < ga->len; i++)
            path.AddLineToPoint(xAt(ga->t[i]), yAt(values[i]));
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(series == 0 ? pal.ra : pal.dec, width).Join(wxJOIN_ROUND)));
        gc->StrokePath(path);
    }

    if (m_hover && *m_hover < ga->len)
    {
        double x = xAt(ga->t[*m_hover]);
        gc->SetPen(wxPen(pal.accent));
        gc->StrokeLine(x, plot.y, x, plot.GetBottom() + 1);
        double r = FromDIP(3);
        gc->SetPen(wxPen(pal.chart, FromDIP(2) / 2));
        for (int series = 0; series < 2; series++)
        {
            if (!(series == 0 ? opt.ra : opt.dec))
                continue;
            gc->SetBrush(series == 0 ? pal.ra : pal.dec);
            double y = yAt((series == 0 ? ga->rac : ga->decc)[*m_hover]);
            gc->DrawEllipse(x - r, y - r, 2 * r, 2 * r);
        }
    }
    gc->ResetClip();
}
