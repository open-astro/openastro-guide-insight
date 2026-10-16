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

#include "FrequencyCanvas.h"

#include "chartdraw.h"
#include "labels.h"
#include "periodogram.h"
#include "ticks.h"

#include <wx/dcbuffer.h>

#include <algorithm>
#include <math.h>
#include <memory>

FrequencyCanvas::FrequencyCanvas(wxWindow *parent, ChartHost& host)
    : m_host(host)
{
    PaintOverParent(this);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, "Frequency");

    Bind(wxEVT_PAINT, &FrequencyCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        m_view.SetWidth(PlotRect().width);
        m_hoverX.reset();
        Refresh();
        e.Skip();
    });
    Bind(wxEVT_MOUSEWHEEL, &FrequencyCanvas::OnWheel, this);
    Bind(wxEVT_MAGNIFY, [this](wxMouseEvent& e) {
        if (Prepare())
        {
            m_view.ZoomAbout(1. + e.GetMagnification(), e.GetX() - PlotRect().x);
            m_hoverX.reset();
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
    Bind(wxEVT_MOTION, &FrequencyCanvas::OnMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) {
        m_hoverX.reset();
        m_host.ShowStatus(wxEmptyString);
        Refresh();
        e.Skip();
    });
}

wxRect FrequencyCanvas::PlotRect() const
{
    wxSize sz = GetClientSize();
    int left = FromDIP(52), right = FromDIP(18), top = FromDIP(14), bottom = FromDIP(28);
    return wxRect(left, top, std::max(sz.x - left - right, 1), std::max(sz.y - top - bottom, 1));
}

const GARun *FrequencyCanvas::Prepare()
{
    const GARun *ga = m_host.Analysis();
    bool usable = ga && ga->nfft >= 2;
    if (m_host.AnalysisGeneration() != m_shown)
    {
        m_shown = m_host.AnalysisGeneration();
        m_hoverX.reset();
        if (usable)
        {
            m_view.SetWidth(PlotRect().width);
            m_view.SetContent(log10(ga->fftx[0]), log10(ga->fftx[ga->nfft - 1]));
            m_top = ga->fftymax * 1.15;
        }
    }
    return usable ? ga : nullptr;
}

double FrequencyCanvas::PeriodAt(const GARun& ga, int column) const
{
    double p = pow(10., m_view.PositionAt(column));
    return std::clamp(p, ga.fftx[0], ga.fftx[ga.nfft - 1]);
}

std::vector<FrequencyCanvas::Column> FrequencyCanvas::Columns(const GARun& ga, const wxRect& plot) const
{
    std::vector<Column> columns((size_t) plot.width);
    std::vector<bool> hasData((size_t) plot.width, false);
    for (size_t i = 0; i < ga.nfft; i++)
    {
        double x = floor(m_view.XAt(log10(ga.fftx[i])));
        if (x < 0. || x >= plot.width)
            continue;
        Column& c = columns[(size_t) x];
        if (!hasData[(size_t) x] || ga.ffty[i] > c.amplitude)
            c = Column{ ga.fftx[i], ga.ffty[i] };
        hasData[(size_t) x] = true;
    }
    for (int x = 0; x < plot.width; x++)
        if (!hasData[(size_t) x])
        {
            double p = PeriodAt(ga, x);
            columns[(size_t) x] = Column{ p, std::max(ga.ffts.Eval(p), 0.) };
        }
    return columns;
}

void FrequencyCanvas::ZoomBy(double factor)
{
    if (Prepare())
    {
        m_view.ZoomAbout(factor, PlotRect().width / 2.);
        m_hoverX.reset();
        Refresh();
    }
}

void FrequencyCanvas::ZoomToFit()
{
    m_shown = 0;
    Prepare();
    Refresh();
}

void FrequencyCanvas::OnWheel(wxMouseEvent& event)
{
    if (!Prepare())
        return;
    double px = WheelPixels(event);
    bool horizontal = event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL;
    if (event.CmdDown() && !horizontal)
        m_view.ZoomAbout(pow(1.1, px / 10.), event.GetX() - PlotRect().x);
    else if (event.AltDown() && !horizontal)
        m_top /= pow(1.1, px / 10.);
    else
        m_view.Scroll(horizontal ? px : -px);
    m_hoverX.reset();
    Refresh();
}

void FrequencyCanvas::OnMotion(wxMouseEvent& event)
{
    const GARun *ga = Prepare();
    if (!ga)
        return;
    if (m_dragX)
    {
        m_view.Scroll(*m_dragX - event.GetX());
        m_dragX = event.GetX();
        m_hoverX.reset();
        Refresh();
        return;
    }

    wxRect plot = PlotRect();
    std::optional<int> hover;
    if (plot.Contains(event.GetPosition()))
    {
        std::vector<double> values;
        for (const Column& c : Columns(*ga, plot))
            values.push_back(c.amplitude);
        hover = (int) SnapToPeak(values, (size_t) (event.GetX() - plot.x), (size_t) FromDIP(8));
    }
    if (hover == m_hoverX)
        return;
    m_hoverX = hover;
    if (hover)
    {
        Column c = Columns(*ga, plot)[(size_t) *hover];
        m_host.ShowStatus(wxString::FromUTF8(PeriodText(c.period, c.amplitude, ga->pixscale,
                                                        m_host.Settings().display.units)));
    }
    else
        m_host.ShowStatus(wxEmptyString);
    Refresh();
}

void FrequencyCanvas::OnPaint(wxPaintEvent&)
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
    auto yAt = [&](double a) { return plot.GetBottom() + 1 - a / m_top * h; };

    // amplitude grid in the chosen units
    bool arcsec = opt.units == Units::ArcSeconds;
    double unitsPerPixel = arcsec ? ga->pixscale : 1.;
    gc->SetFont(GetFont().Smaller(), pal.text2);
    double step = NiceStep(m_top * unitsPerPixel / 6.);
    for (double v : StepsBetween(0., m_top * unitsPerPixel, step))
    {
        double y = yAt(v / unitsPerPixel);
        gc->SetPen(wxPen(v == 0. ? pal.zero : pal.grid));
        gc->StrokeLine(plot.x, y, plot.GetRight() + 1, y);
        wxString label = wxString::FromUTF8(Fixed(v, step >= 1. ? 0 : step >= 0.1 ? 1 : 2) + (arcsec ? "″" : ""));
        double tw, th;
        gc->GetTextExtent(label, &tw, &th);
        gc->DrawText(label, plot.x - tw - FromDIP(6), y - th / 2.);
    }

    // periods along the bottom
    double p0 = pow(10., m_view.First()), p1 = pow(10., m_view.Last());
    std::vector<double> ticks = LogTicks(p0, p1);
    std::vector<double> xs, widths;
    for (double p : ticks)
    {
        xs.push_back(plot.x + m_view.XAt(log10(p)));
        double tw, th;
        gc->GetTextExtent(wxString::Format("%g s", p), &tw, &th);
        widths.push_back(tw);
    }
    std::vector<double> lefts;
    for (size_t i = 0; i < xs.size(); i++)
        lefts.push_back(xs[i] - widths[i] / 2.);
    std::vector<bool> fits = LabelsThatFit(lefts, widths, FromDIP(10));
    for (size_t i = 0; i < ticks.size(); i++)
    {
        gc->SetPen(wxPen(pal.grid));
        gc->StrokeLine(xs[i], plot.y, xs[i], plot.GetBottom() + 1);
        if (fits[i])
            gc->DrawText(wxString::Format("%g s", ticks[i]), lefts[i], plot.GetBottom() + FromDIP(6));
    }

    gc->Clip(plot.x, plot.y, plot.width, plot.height);

    // the periodogram, one point per column
    std::vector<Column> columns = Columns(*ga, plot);
    wxGraphicsPath path = gc->CreatePath();
    for (int x = 0; x < plot.width; x++)
    {
        double y = yAt(columns[(size_t) x].amplitude);
        if (x == 0)
            path.MoveToPoint(plot.x, y);
        else
            path.AddLineToPoint(plot.x + x, y);
    }
    gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(pal.ra, FromDIP(16) / 10.).Join(wxJOIN_ROUND)));
    gc->StrokePath(path);

    auto marker = [&](double x, double a, const wxString& label) {
        double y = yAt(a), r = FromDIP(9) / 2.;
        gc->SetPen(wxPen(pal.accent, FromDIP(2)));
        gc->SetBrush(pal.chart);
        gc->DrawEllipse(x - r, y - r, 2 * r, 2 * r);
        if (label.empty())
            return;
        gc->SetFont(GetFont().Smaller().Bold(), pal.text);
        double tw, th;
        gc->GetTextExtent(label, &tw, &th);
        double lx = std::min(x + FromDIP(8), plot.GetRight() - tw - FromDIP(4));
        double ly = std::max(y - th - FromDIP(8), (double) plot.y + FromDIP(2));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(WithAlpha(pal.chart, 220));
        gc->DrawRoundedRectangle(lx - FromDIP(3), ly - 1, tw + FromDIP(6), th + 2, FromDIP(3));
        gc->DrawText(label, lx, ly);
    };

    // the largest peak, and the one under the pointer
    if (std::optional<size_t> peak = LargestPeak(*ga))
    {
        double p = ga->fftx[*peak];
        if (p >= p0 && p <= p1)
            marker(plot.x + m_view.XAt(log10(p)), ga->ffty[*peak],
                   wxString::FromUTF8(Fixed(p, p < 100. ? 1 : 0) + " s · " +
                                      Distance(ga->ffty[*peak], ga->pixscale, opt.units)));
    }
    if (m_hoverX && *m_hoverX < plot.width)
    {
        double x = plot.x + *m_hoverX;
        gc->SetPen(wxPen(WithAlpha(pal.accent, 140)));
        gc->StrokeLine(x, plot.y, x, plot.GetBottom() + 1);
        marker(x, columns[(size_t) *m_hoverX].amplitude, wxString());
    }
    gc->ResetClip();
}
