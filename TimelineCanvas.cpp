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

#include "TimelineCanvas.h"

#include "chartdraw.h"
#include "commands.h"
#include "labels.h"
#include "ticks.h"

#include <wx/dcbuffer.h>
#include <wx/menu.h>

#include <algorithm>
#include <math.h>
#include <memory>

namespace {

const int DRAG_THRESHOLD = 3;

// The zoom for one wheel notch, as the old window's + and - buttons.
const double ZOOM_STEP = 1.1;

wxString U(const std::string& utf8)
{
    return wxString::FromUTF8(utf8);
}

} // namespace

TimelineCanvas::TimelineCanvas(wxWindow *parent, ChartHost& host)
    : m_host(host)
{
    PaintOverParent(this);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS, "Timeline");

    Bind(wxEVT_PAINT, &TimelineCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &TimelineCanvas::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &TimelineCanvas::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &TimelineCanvas::OnLeftUp, this);
    Bind(wxEVT_MOTION, &TimelineCanvas::OnMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, &TimelineCanvas::OnLeave, this);
    Bind(wxEVT_MOUSEWHEEL, &TimelineCanvas::OnWheel, this);
    Bind(wxEVT_MAGNIFY, &TimelineCanvas::OnMagnify, this);
    Bind(wxEVT_KEY_DOWN, &TimelineCanvas::OnKeyDown, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &TimelineCanvas::OnCaptureLost, this);
    Bind(wxEVT_CONTEXT_MENU, &TimelineCanvas::OnContextMenu, this);
}

wxRect TimelineCanvas::PlotRect() const
{
    wxSize sz = GetClientSize();
    int left = FromDIP(52), right = FromDIP(14), top = FromDIP(26), bottom = FromDIP(10);
    return wxRect(left, top, std::max(sz.x - left - right, 1), std::max(sz.y - top - bottom, 1));
}

SessionView *TimelineCanvas::View()
{
    Document& doc = m_host.Doc();
    SessionView *v = doc.View();
    if (!v)
        return nullptr;

    wxRect plot = PlotRect();
    if (v->horizontal.Width() != plot.width)
        v->horizontal.SetWidth(plot.width);
    if (!v->verticalFitted)
    {
        if (doc.VerticalScaleLocked())
            v->verticalFitted = true;
        else
            doc.FitVertical(plot.height);
    }
    return v;
}

double TimelineCanvas::XAtIndex(double index)
{
    SessionView *v = View();
    const GuideSession *s = m_host.Doc().Session();
    if (!v || !s)
        return 0.;
    return PlotRect().x + v->horizontal.XAt(PositionAtIndex(s->entries, index, m_host.Doc().GetXAxis()));
}

double TimelineCanvas::PositionAtX(int x)
{
    SessionView *v = View();
    return v ? v->horizontal.PositionAt(x - PlotRect().x) : 0.;
}

void TimelineCanvas::OnSize(wxSizeEvent& event)
{
    // the visible range can change with the width; let the heading catch up
    if (View())
        CallAfter([this] { m_host.ViewChanged(); });
    Refresh();
    event.Skip();
}

void TimelineCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const Palette& pal = m_host.Colors();
    ClearToParent(this, dc);

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
        return;

    DrawPanel(*gc, this, pal);

    if (m_host.Doc().Session() && View())
        Paint(*gc, PlotRect());
}

void TimelineCanvas::Paint(wxGraphicsContext& gc, const wxRect& plot)
{
    Document& doc = m_host.Doc();
    const GuideSession& s = *doc.Session();
    const auto& entries = s.entries;
    const DisplayOptions& opt = m_host.Settings().display;
    const Palette& pal = m_host.Colors();
    SessionView& v = *View();
    XAxis axis = doc.GetXAxis();
    double h = plot.height;

    auto yAt = [&](double value) { return plot.y + v.vertical.YAt(value, h); };
    double y0 = yAt(0.);

    std::optional<IndexRun> frames = FramesToDraw(entries, v.horizontal.First(), v.horizontal.Last(), axis);

    if (opt.grid)
    {
        PaintGrid(gc, plot, y0);
        PaintTimeTicks(gc, plot);
    }

    gc.Clip(plot.x, plot.y, plot.width, plot.height);

    // excluded frames
    if (frames)
    {
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(pal.excluded);
        for (const IndexRun& run : ExcludedRuns(entries, frames->first, frames->last))
        {
            double x0 = XAtIndex(run.first - 0.5), x1 = XAtIndex(run.last + 0.5);
            gc.DrawRectangle(x0, plot.y, std::max(x1 - x0, 1.), h);
        }
    }

    // the selection, or the range being dragged
    if (doc.Selection())
        PaintRange(gc, plot, XAtIndex(doc.Selection()->first - 0.5), XAtIndex(doc.Selection()->last + 0.5),
                   pal.viewFill, pal.accent);
    if (m_drag != Drag::None && m_drag != Drag::PanVertical && m_moved)
    {
        wxColour fill = m_drag == Drag::Exclude ? WithAlpha(pal.text2, 60) :
                        m_drag == Drag::Include ? WithAlpha(pal.accent, 50) : pal.viewFill;
        PaintRange(gc, plot, std::min(m_anchorX, m_dragX), std::max(m_anchorX, m_dragX), fill,
                   m_drag == Drag::Select ? pal.accent : wxTransparentColour);
    }

    // zero line
    gc.SetPen(wxPen(pal.zero));
    gc.StrokeLine(plot.x, y0, plot.GetRight() + 1, y0);

    // guide limits
    if (opt.limits)
    {
        for (Series series : { Series::Ra, Series::Dec })
        {
            if (!(series == Series::Ra ? opt.ra : opt.dec))
                continue;
            AxisLimits lim = LimitsFor(s, series, opt.device);
            wxColour c = series == Series::Ra ? pal.ra : pal.dec;
            gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(WithAlpha(c, 150)).Style(wxPENSTYLE_SHORT_DASH)));
            for (double d : { lim.minMove, lim.maxPulse })
                if (d > 0.)
                    for (double sign : { 1., -1. })
                        gc.StrokeLine(plot.x, yAt(sign * d), plot.GetRight() + 1, yAt(sign * d));
        }
    }

    if (!frames)
        return;

    // distance between neighbouring frames on screen
    double spacing = (XAtIndex(frames->last) - XAtIndex(frames->first)) /
                     std::max<double>(frames->last - frames->first, 1.);

    // guide pulses as bars from the zero line, RA left of each frame and Dec right
    if (opt.corrections && spacing >= 2.)
    {
        double w = std::min(spacing * 0.4, (double) FromDIP(8));
        for (Series series : { Series::Ra, Series::Dec })
        {
            if (!(series == Series::Ra ? opt.ra : opt.dec))
                continue;
            wxGraphicsPath bars = gc.CreatePath();
            for (unsigned int i = frames->first; i <= frames->last; i++)
            {
                double c = CorrectionValue(s, entries[i], series, opt.device);
                if (c == 0.)
                    continue;
                double x = XAtIndex(i) + (series == Series::Ra ? -w : 0.);
                double y = yAt(c);
                bars.AddRectangle(x, std::min(y, y0), w, std::max(fabs(y - y0), 1.));
            }
            gc.SetPen(*wxTRANSPARENT_PEN);
            gc.SetBrush(series == Series::Ra ? pal.raFill : pal.decFill);
            gc.FillPath(bars);
        }
    }

    std::vector<IndexRun> runs = FoundRuns(entries, frames->first, frames->last);
    auto drawSeries = [&](const wxColour& colour, double width, auto value) {
        gc.SetPen(gc.CreatePen(wxGraphicsPenInfo(colour, width).Join(wxJOIN_ROUND)));
        gc.SetBrush(colour);
        for (const IndexRun& run : runs)
        {
            if (run.first == run.last)
            {
                double r = width;
                gc.DrawEllipse(XAtIndex(run.first) - r, value(run.first) - r, 2 * r, 2 * r);
                continue;
            }
            wxGraphicsPath path = gc.CreatePath();
            path.MoveToPoint(XAtIndex(run.first), value(run.first));
            for (unsigned int i = run.first + 1; i <= run.last; i++)
                path.AddLineToPoint(XAtIndex(i), value(i));
            gc.StrokePath(path);
        }
    };

    double lineWidth = spacing >= 3. ? FromDIP(16) / 10. : FromDIP(10) / 10.;

    // star mass and SNR, each scaled to fill the upper half
    if (opt.mass || opt.snr)
    {
        double maxMass = 0., maxSnr = 0.;
        for (const auto& e : entries)
        {
            maxMass = std::max(maxMass, (double) e.mass);
            maxSnr = std::max(maxSnr, (double) e.snr);
        }
        double top = (h / 2. - FromDIP(10));
        if (opt.mass && maxMass > 0.)
            drawSeries(pal.dark ? wxColour(0xe8, 0xc5, 0x47) : wxColour(0xb8, 0x8a, 0x00), lineWidth,
                       [&](unsigned int i) { return plot.y + h / 2. - entries[i].mass / maxMass * top; });
        if (opt.snr && maxSnr > 0.)
            drawSeries(pal.text2, lineWidth,
                       [&](unsigned int i) { return plot.y + h / 2. - entries[i].snr / maxSnr * top; });
    }

    if (opt.ra)
        drawSeries(pal.ra, lineWidth, [&](unsigned int i) { return yAt(SeriesValue(entries[i], Series::Ra, opt.raDecAxes)); });
    if (opt.dec)
        drawSeries(pal.dec, lineWidth, [&](unsigned int i) { return yAt(SeriesValue(entries[i], Series::Dec, opt.raDecAxes)); });

    if (opt.events)
        PaintEvents(gc, plot, *frames);

    // the frame under the pointer
    if (m_hover && *m_hover < entries.size())
    {
        double x = XAtIndex((double) *m_hover);
        gc.SetPen(wxPen(pal.accent));
        gc.StrokeLine(x, plot.y, x, plot.GetBottom() + 1);
        const GuideEntry& e = entries[*m_hover];
        if (StarWasFound(e.err))
        {
            double r = FromDIP(3);
            gc.SetPen(wxPen(pal.chart, FromDIP(2) / 2));
            for (Series series : { Series::Ra, Series::Dec })
            {
                if (!(series == Series::Ra ? opt.ra : opt.dec))
                    continue;
                gc.SetBrush(series == Series::Ra ? pal.ra : pal.dec);
                double y = yAt(SeriesValue(e, series, opt.raDecAxes));
                gc.DrawEllipse(x - r, y - r, 2 * r, 2 * r);
            }
        }
    }

    gc.ResetClip();
}

void TimelineCanvas::PaintRange(wxGraphicsContext& gc, const wxRect& plot, double x0, double x1,
                                const wxColour& fill, const wxColour& edge)
{
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(fill);
    gc.DrawRectangle(x0, plot.y, std::max(x1 - x0, 1.), plot.height);
    if (edge.IsOk() && edge.Alpha() != 0)
    {
        gc.SetPen(wxPen(edge));
        gc.StrokeLine(x0, plot.y, x0, plot.GetBottom() + 1);
        gc.StrokeLine(x1, plot.y, x1, plot.GetBottom() + 1);
    }
}

void TimelineCanvas::PaintGrid(wxGraphicsContext& gc, const wxRect& plot, double y0)
{
    const GuideSession& s = *m_host.Doc().Session();
    const DisplayOptions& opt = m_host.Settings().display;
    const Palette& pal = m_host.Colors();
    SessionView& v = *View();

    // grid steps in the chosen units, about 12 to the chart's height
    bool arcsec = opt.units == Units::ArcSeconds;
    double unitsPerPixel = arcsec ? s.pixelScale : 1.; // display units per guide pixel
    double pxPerUnit = v.vertical.Scale() / unitsPerPixel;
    if (!(pxPerUnit > 0.))
        return;
    double step = NiceStep(plot.height / 12. / pxPerUnit);

    double top = (y0 - plot.y) / pxPerUnit;           // value at the top, in display units
    double bottom = -(plot.GetBottom() - y0) / pxPerUnit;
    gc.SetFont(GetFont().Smaller(), pal.text2);
    for (double value : StepsBetween(bottom, top, step))
    {
        double y = y0 - value * pxPerUnit;
        if (fabs(value) > step / 2.)
        {
            gc.SetPen(wxPen(pal.grid));
            gc.StrokeLine(plot.x, y, plot.GetRight() + 1, y);
        }
        int decimals = step >= 1. ? 0 : step >= 0.1 ? 1 : 2;
        wxString label = U(Fixed(value, decimals) + (arcsec ? "″" : ""));
        double tw, th;
        gc.GetTextExtent(label, &tw, &th);
        gc.DrawText(label, plot.x - tw - FromDIP(6), y - th / 2.);
    }
}

void TimelineCanvas::PaintTimeTicks(wxGraphicsContext& gc, const wxRect& plot)
{
    const GuideSession& s = *m_host.Doc().Session();
    const auto& entries = s.entries;
    const Palette& pal = m_host.Colors();
    SessionView& v = *View();
    XAxis axis = m_host.Doc().GetXAxis();
    if (entries.size() < 2)
        return;

    // the visible span in seconds from the start of the session
    double t0 = PositionAtIndex(entries, IndexAtPosition(entries, v.horizontal.First(), axis), XAxis::Time);
    double t1 = PositionAtIndex(entries, IndexAtPosition(entries, v.horizontal.Last(), axis), XAxis::Time);
    if (!(t1 > t0))
        return;

    double step = TimeStep(FromDIP(80) * (t1 - t0) / plot.width);
    std::vector<TimeTick> ticks = TimeTicks(s.starts, t0, t1, step);

    gc.SetFont(GetFont().Smaller(), pal.text2);
    std::vector<double> xs, widths;
    for (const TimeTick& t : ticks)
    {
        double pos = axis == XAxis::Time ? t.t : IndexAtTime(entries, t.t);
        xs.push_back(plot.x + v.horizontal.XAt(pos));
        double tw, th;
        gc.GetTextExtent(U(t.label), &tw, &th);
        widths.push_back(tw);
    }
    // ticks bunch up over a pause on the frame axis; label only those with room
    std::vector<bool> fits = LabelsThatFit(xs, widths, FromDIP(8));

    gc.SetPen(wxPen(pal.grid));
    for (size_t i = 0; i < ticks.size(); i++)
    {
        gc.StrokeLine(xs[i], plot.y - FromDIP(6), xs[i], plot.GetBottom() + 1);
        if (fits[i])
            gc.DrawText(U(ticks[i].label), xs[i] + FromDIP(3), plot.y - FromDIP(22));
    }
}

void TimelineCanvas::PaintEvents(wxGraphicsContext& gc, const wxRect& plot, const IndexRun& frames)
{
    const GuideSession& s = *m_host.Doc().Session();
    const Palette& pal = m_host.Colors();

    std::vector<double> xs, widths;
    std::vector<wxString> texts;
    gc.SetFont(GetFont().Smaller(), pal.text2);
    for (const InfoEntry& info : s.infos)
    {
        if (info.idx < (int) frames.first || info.idx > (int) frames.last + 1)
            continue;
        wxString text = U(info.repeats > 1 ? std::to_string(info.repeats) + " × " + info.info : info.info);
        double tw, th;
        gc.GetTextExtent(text, &tw, &th);
        // events happen before the frame they are logged with
        xs.push_back(XAtIndex(info.idx - 0.5));
        widths.push_back(tw + FromDIP(12));
        texts.push_back(text);
    }

    std::vector<int> rows = StackLabels(xs, widths, FromDIP(4));
    double rowHeight = GetCharHeight() + FromDIP(6);
    int maxRows = std::max(1, (int) (plot.height / 3 / rowHeight));

    wxGraphicsPen dashed = gc.CreatePen(wxGraphicsPenInfo(WithAlpha(pal.text2, 140)).Style(wxPENSTYLE_SHORT_DASH));
    for (size_t i = 0; i < xs.size(); i++)
    {
        gc.SetPen(dashed);
        gc.StrokeLine(xs[i], plot.y, xs[i], plot.GetBottom() + 1);
        if (rows[i] >= maxRows)
            continue;
        double y = plot.GetBottom() - FromDIP(4) - (rows[i] + 1) * rowHeight;
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(WithAlpha(pal.segBg, 230));
        gc.DrawRoundedRectangle(xs[i] + FromDIP(3), y, widths[i] - FromDIP(4), rowHeight - FromDIP(2), FromDIP(4));
        double tw, th;
        gc.GetTextExtent(texts[i], &tw, &th);
        gc.DrawText(texts[i], xs[i] + FromDIP(8), y + (rowHeight - FromDIP(2) - th) / 2.);
    }
}

void TimelineCanvas::SetHover(std::optional<size_t> frame)
{
    if (frame == m_hover)
        return;
    m_hover = frame;
    const GuideSession *s = m_host.Doc().Session();
    if (frame && s)
        m_host.ShowStatus(U(HoverText(*s, *frame, m_host.Settings().display.units, m_host.Settings().display.raDecAxes)));
    else
        m_host.ShowStatus(wxEmptyString);
    Refresh();
}

void TimelineCanvas::OnLeftDown(wxMouseEvent& event)
{
    SetFocus();
    if (!m_host.Doc().Session())
        return;

    m_anchorX = m_dragX = event.GetX();
    m_lastY = event.GetY();
    m_moved = false;
    if (event.AltDown())
        m_drag = Drag::PanVertical;
    else if (event.CmdDown())
        m_drag = Drag::Exclude;
    else if (event.ShiftDown())
        m_drag = Drag::Include;
    else
        m_drag = Drag::Select;
    if (!HasCapture())
        CaptureMouse();
}

void TimelineCanvas::OnMotion(wxMouseEvent& event)
{
    const GuideSession *s = m_host.Doc().Session();
    if (!s)
        return;

    if (m_drag == Drag::None)
    {
        SetHover(PlotRect().Contains(event.GetPosition()) ?
                 FrameAt(s->entries, PositionAtX(event.GetX()), m_host.Doc().GetXAxis()) : std::nullopt);
        return;
    }

    if (abs(event.GetX() - m_anchorX) > DRAG_THRESHOLD || abs(event.GetY() - m_lastY) > DRAG_THRESHOLD)
        m_moved = true;

    if (m_drag == Drag::PanVertical)
    {
        if (SessionView *v = View())
            v->vertical.Pan(event.GetY() - m_lastY);
        m_lastY = event.GetY();
        Refresh();
        return;
    }

    wxRect plot = PlotRect();
    m_dragX = std::clamp(event.GetX(), plot.x, plot.GetRight());
    Refresh();
}

void TimelineCanvas::OnLeftUp(wxMouseEvent& event)
{
    if (m_drag == Drag::None)
        return;

    Drag drag = m_drag;
    bool moved = m_moved;
    EndDrag();

    Document& doc = m_host.Doc();
    const GuideSession *s = doc.Session();
    if (!s || drag == Drag::PanVertical)
        return;
    XAxis axis = doc.GetXAxis();

    if (moved)
    {
        std::optional<IndexRun> run = FramesBetween(s->entries, PositionAtX(m_anchorX), PositionAtX(m_dragX), axis);
        if (drag == Drag::Select)
        {
            doc.SetSelection(run);
            m_host.SelectionChanged();
        }
        else if (run)
        {
            doc.IncludeFrames(*run, drag == Drag::Include);
            m_host.FramesChanged();
        }
        return;
    }

    // a click: Command-click on excluded frames includes their whole run
    std::optional<size_t> frame = FrameAt(s->entries, PositionAtX(event.GetX()), axis);
    if (drag == Drag::Exclude && frame && !s->entries[*frame].included)
    {
        for (const IndexRun& run : ExcludedRuns(s->entries, 0, (unsigned int) s->entries.size() - 1))
            if (run.first <= *frame && *frame <= run.last)
            {
                doc.IncludeFrames(run, true);
                m_host.FramesChanged();
            }
        return;
    }
    if (drag == Drag::Select && doc.Selection())
    {
        doc.SetSelection(std::nullopt);
        m_host.SelectionChanged();
    }
}

void TimelineCanvas::EndDrag()
{
    m_drag = Drag::None;
    m_moved = false;
    if (HasCapture())
        ReleaseMouse();
    Refresh();
}

void TimelineCanvas::OnCaptureLost(wxMouseCaptureLostEvent&)
{
    m_drag = Drag::None;
    m_moved = false;
    Refresh();
}

void TimelineCanvas::OnLeave(wxMouseEvent& event)
{
    if (m_drag == Drag::None)
        SetHover(std::nullopt);
    event.Skip();
}

void TimelineCanvas::OnWheel(wxMouseEvent& event)
{
    SessionView *v = View();
    if (!v)
        return;
    double px = WheelPixels(event);
    bool horizontal = event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL;

    if (event.CmdDown() && !horizontal)
    {
        // zoom about the pointer
        v->horizontal.ZoomAbout(pow(ZOOM_STEP, px / 10.), event.GetX() - PlotRect().x);
    }
    else if (event.AltDown() && !horizontal)
    {
        if (m_host.Doc().VerticalScaleLocked())
            m_host.Doc().ZoomVertical(pow(ZOOM_STEP, px / 10.));
        else
            v->vertical.Zoom(pow(ZOOM_STEP, px / 10.));
    }
    else
        v->horizontal.Scroll(horizontal ? px : -px);

    m_hover.reset();
    m_host.ViewChanged();
}

void TimelineCanvas::OnMagnify(wxMouseEvent& event)
{
    SessionView *v = View();
    if (!v)
        return;
    v->horizontal.ZoomAbout(1. + event.GetMagnification(), event.GetX() - PlotRect().x);
    m_host.ViewChanged();
}

void TimelineCanvas::OnKeyDown(wxKeyEvent& event)
{
    SessionView *v = View();
    switch (event.GetKeyCode())
    {
    case WXK_LEFT:
    case WXK_RIGHT:
        if (v)
        {
            double step = PlotRect().width / 8.;
            v->horizontal.Scroll(event.GetKeyCode() == WXK_LEFT ? -step : step);
            m_host.ViewChanged();
        }
        break;
    case WXK_ESCAPE:
        if (m_drag != Drag::None)
            EndDrag();
        else if (m_host.Doc().Selection())
        {
            m_host.Doc().SetSelection(std::nullopt);
            m_host.SelectionChanged();
        }
        break;
    case WXK_TAB:
        Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward);
        break;
    default:
        event.Skip();
    }
}

void TimelineCanvas::OnContextMenu(wxContextMenuEvent& event)
{
    if (!m_host.Doc().Session())
        return;

    // the main window handles these, and enables them as its menus do
    wxMenu menu;
    menu.Append(ID_EXCLUDE_SELECTION, "Exclude Selected Frames");
    menu.Append(ID_INCLUDE_SELECTION, "Include Selected Frames");
    menu.Append(ID_ZOOM_SELECTION, "Zoom to Selection");
    menu.Append(ID_DESELECT, "Deselect");
    menu.AppendSeparator();
    // check items: the main window checks the view being shown
    menu.AppendCheckItem(ID_VIEW_DRIFT, "Show Drift");
    menu.AppendCheckItem(ID_VIEW_FREQUENCY, "Show Frequency");
    menu.Append(ID_SELECT_GA, "Select Guiding Assistant Run");
    menu.AppendSeparator();
    menu.Append(ID_INCLUDE_ALL, "Include All Frames");
    menu.Append(ID_EXCLUDE_ALL, "Exclude All Frames");
    menu.Append(ID_REAPPLY_RULES, "Reapply Exclusion Rules");

    wxPoint pos = event.GetPosition() == wxDefaultPosition ? wxDefaultPosition : ScreenToClient(event.GetPosition());
    PopupMenu(&menu, pos);
}

void TimelineCanvas::ZoomBy(double factor)
{
    if (SessionView *v = View())
    {
        // about the middle, or the selection when it is in view
        double x = PlotRect().width / 2.;
        v->horizontal.ZoomAbout(factor, x);
        m_host.ViewChanged();
    }
}

bool TimelineCanvas::CanZoomIn()
{
    SessionView *v = View();
    return v && v->horizontal.CanZoomIn();
}

bool TimelineCanvas::CanZoomOut()
{
    SessionView *v = View();
    return v && v->horizontal.CanZoomOut();
}

void TimelineCanvas::ZoomToFit()
{
    if (SessionView *v = View())
    {
        v->horizontal.Fit();
        m_host.ViewChanged();
    }
}

void TimelineCanvas::ZoomToSelection()
{
    Document& doc = m_host.Doc();
    SessionView *v = View();
    if (!v || !doc.Selection())
        return;
    const auto& entries = doc.Session()->entries;
    XAxis axis = doc.GetXAxis();
    v->horizontal.Show(PositionAtIndex(entries, doc.Selection()->first - 0.5, axis),
                       PositionAtIndex(entries, doc.Selection()->last + 0.5, axis));
    m_host.ViewChanged();
}

void TimelineCanvas::FitVertical()
{
    if (View())
    {
        m_host.Doc().FitVertical(PlotRect().height);
        m_host.ViewChanged();
    }
}
