/*
 * This file is part of phdlogview
 *
 * Copyright (C) 2018 Andy Galasso
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, visit the http://fsf.org website.
 */

#include "AnalysisWin.h"

#include "logparser.h"
#include "LogViewApp.h"

#include <algorithm>
#include <wx/dcbuffer.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

struct DragInfo
{
    bool dragging;
    DragDirection drag_direction;
    wxPoint dragStart;
    wxPoint dragLast;
    double dragStartXofs;
};
static DragInfo s_drag;

struct GraphPos
{
    int x0, y0, x1, y1; // screen coords of upper left and lower right corners of graph (accomodates padding)
    double xofs; // panning offset
    double scx, scy; // current scaling factor (zoom)
    const GARun *ga;
};

struct FFTPos : public GraphPos
{
    enum { PADX = 10, PADY = 4, };

    int xw; // width of spline domain in screen coordinates
    double p0, p1; // bounds of spline domain

    void Init(const wxSize& sz, const GARun& ga_)
    {
        ga = &ga_;
        HReset(sz.x);
        VReset(sz.y);
    }
    void HReset(int width)
    {
        x0 = PADX;
        xofs = (double) x0;
        x1 = width - PADX;
        xw = x1 - x0;

        p0 = ga->fftx[0];
        p1 = ga->fftx[ga->nfft - 1];
        scx = log(p1 / p0) / (double) xw;
    }
    void VReset(int height)
    {
        y0 = height - PADY;
        y1 = PADY;
        scy = 0.8 * (double) (y0 - y1) / ga->fftymax;
    }
    void Resize(const wxSize& sz)
    {
        // x-axis
        double k0 = ((double) x0 - xofs) * scx;
        double k1 = ((double) x1 - xofs) * scx;
        x1 = sz.x - PADX;
        scx = (k1 - k0) / (double) (x1 - x0);
        xofs = (double) x1 - k1 / scx;
        xw = (int) floor(log(p1 / p0) / scx);

        // y-axis
        int ty0 = sz.y - PADY;
        scy *= (double) (ty0 - y1) / (double) (y0 - y1);
        y0 = ty0;
    }
    void HZoom(double d, int center)
    {
        if (center < 0)
            center = (x0 + x1) / 2;
        double tscx = scx * d;
        xofs = (double) center - scx * ((double) center - xofs) / tscx;
        scx = tscx;
        xw = (int) floor(log(p1 / p0) / scx);
    }
    void VZoom(double d)
    {
        scy *= d;
    }
    double P(int x) const
    {
        return p0 * exp(((double) x - xofs) * scx);
    }
    int X(double p)
    {
        return (int)(floor(log(p / p0) / scx + xofs));
    }
    int Eval(int x)
    {
        double p = P(x);
        double a = ga->ffts.Eval(p);
        return y0 - (int)(a * scy);
    }
    double FEval(int x)
    {
        double p = P(x);
        return ga->ffts.Eval(p);
    }
    void Eval(int x, int *y, double *pp, double *pa)
    {
        double p = P(x);
        double a = ga->ffts.Eval(p);
        *y = y0 - (int)(a * scy);
        *pp = p;
        *pa = a;
    }
    int StartX() const
    {
        return std::max(x0, (int) ceil(xofs));
    }
    int EndX() const
    {
        return std::min(x1, (int) floor(xofs) + xw);
    }
};
static FFTPos s_fftpos;

struct DriftPos : public GraphPos
{
    enum { PADX = 10, PADY = 10, };

    double ymax;

    void Init(const wxSize& sz, const GARun& ga_)
    {
        ga = &ga_;

        size_t const n = ga->len;
        ymax = fabs(ga->rac[0]);
        for (auto it = &ga->rac[1]; it != &ga->rac[n]; ++it)
        {
            double y = fabs(*it);
            if (y > ymax) ymax = y;
        }
        for (auto it = &ga->decc[0]; it != &ga->decc[n]; ++it)
        {
            double y = fabs(*it);
            if (y > ymax) ymax = y;
        }

        HReset(sz.x);
        VReset(sz.y);
    }
    double T(int x)
    {
        return ((double) x - xofs) * scx;
    }
    int X(double t)
    {
        return (int) (t / scx + xofs);
    }
    double RaOrDec(int y)
    {
        int const ymid = (y0 + y1) / 2;
        return (double)(y - ymid) / scy;
    }
    void HReset(int width)
    {
        x0 = PADX;
        x1 = width - PADX;
        size_t const n = ga->len;
        double const tmin = ga->t[0];
        double const tmax = ga->t[n - 1];
        scx = (tmax - tmin) / (double) (x1 - x0);
        xofs = (double) x0 - tmin / scx;
    }
    void VReset(int height)
    {
        y0 = height - PADY;
        y1 = PADY;
        scy = 0.9 * (double) (y0 - y1) / (2.0 * ymax);
    }
    void Resize(const wxSize& sz)
    {
        int tx1 = sz.x - PADX;
        double tscx = scx * (double) (x1 - x0) / (double) (tx1 - x0);
        xofs = x0 - scx * ((double) x0 - xofs) / tscx;
        scx = tscx;
        x1 = tx1;

        double ym = 0.5 * (double)(y0 + y1);
        int ty0 = sz.y - PADY;
        double tym = 0.5 * (double)(ty0 + y1);
        scy *= ((double) ty0 - tym) / ((double) y0 - ym);
        y0 = ty0;
    }
    void HZoom(double d, int center)
    {
        if (center < 0)
            center = (x0 + x1) / 2;
        double tscx = scx * d;
        xofs = (double) center - scx * ((double) center - xofs) / tscx;
        scx = tscx;
    }
    void VZoom(double d)
    {
        scy *= d;
    }
};
static DriftPos s_drpos;

wxBEGIN_EVENT_TABLE(AnalysisWin, AnalyzeFrameBase)
  EVT_MOUSEWHEEL(AnalysisWin::OnMouseWheel)
wxEND_EVENT_TABLE()

AnalysisWin::AnalysisWin(LogViewFrame *parent)
    :
    AnalyzeFrameBase(parent),
    m_cursor(-1)
{
    m_graph->SetBackgroundStyle(wxBG_STYLE_PAINT);
    LoadGeometry(this, "/geometry.awin");

    m_graph->Connect(wxEVT_MOUSE_CAPTURE_LOST, wxMouseCaptureLostEventHandler(AnalysisWin::OnCaptureLost), nullptr, this);

    // Add "Save CSV" button to the top toolbar; visible only in Frequency Analysis mode
    wxSizer* topSizer = GetSizer()->GetItem((size_t)0)->GetSizer();
    m_saveCSV = new wxButton(this, wxID_ANY, _("Save CSV"));
    topSizer->Add(m_saveCSV, 0, wxALL, 5);
    m_saveCSV->Hide(); // hidden initially (Drift view is active by default)
    m_saveCSV->Bind(wxEVT_BUTTON, &AnalysisWin::OnSaveCSV, this);
}

AnalysisWin::~AnalysisWin()
{
    LogViewFrame *win = static_cast<LogViewFrame *>(GetParent());
    win->m_analysisWin = nullptr;
}

static void GetGABounds(const GuideSession& session, size_t pos, size_t *begin, size_t *end)
{
    const auto& entries = session.entries;
    assert(!entries[pos].guiding);
    size_t p = pos;
    while (true)
    {
        if (p == 0 || entries[p - 1].guiding)
        {
            *begin = p;
            break;
        }
        --p;
    }
    p = pos + 1;
    while (true)
    {
        if (p >= entries.size() || entries[p].guiding)
        {
            *end = p;
            break;
        }
        ++p;
    }
}

bool AnalysisWin::CanAnalyzeGA(const GuideSession& session, size_t pos)
{
    if (pos >= session.entries.size() || session.entries[pos].guiding)
        return false;
    size_t begin, end;
    GetGABounds(session, pos, &begin, &end);
    return GARun::CanAnalyze(session, begin, end);
}

void AnalysisWin::AnalyzeGA(const GuideSession& session, size_t pos)
{
    size_t begin, end;
    GetGABounds(session, pos, &begin, &end);
    m_garun.Analyze(session, begin, end, false);
    s_drpos.Init(m_graph->GetSize(), m_garun);
    s_fftpos.Init(m_graph->GetSize(), m_garun);
    SetTitle(_("Analysis"));
}

bool AnalysisWin::CanAnalyzeAll(const GuideSession& session)
{
    return GARun::CanAnalyze(session, 0, session.entries.size());
}

void AnalysisWin::AnalyzeAll(const GuideSession& session, bool undo_ra_corrections)
{
    m_garun.Analyze(session, 0, session.entries.size(), undo_ra_corrections);
    s_drpos.Init(m_graph->GetSize(), m_garun);
    s_fftpos.Init(m_graph->GetSize(), m_garun);
    SetTitle(undo_ra_corrections ? _("Analysis ** RA Corrections Removed **") : _("Analysis"));
}

void AnalysisWin::OnCheck(wxCommandEvent& event)
{
    m_graph->Refresh();
}

void AnalysisWin::OnClose(wxCloseEvent& event)
{
    ::SaveGeometry(this, "/geometry.awin");
    event.Skip();
}

void AnalysisWin::OnSizeGraph(wxSizeEvent& event)
{
    s_drpos.Resize(m_graph->GetSize());
    s_fftpos.Resize(m_graph->GetSize());
    m_cursor = -1;
    m_graph->Refresh();
}

void AnalysisWin::OnBtnLeftDown(wxMouseEvent& event)
{
    wxToggleButton *btn = reinterpret_cast<wxToggleButton *>(event.GetEventObject());
    if (!btn->GetValue())
    {
        event.Skip();
        return;
    }
}

void AnalysisWin::OnClickDrift(wxCommandEvent& event)
{
    m_toggleFFT->SetValue(false);
    m_graph->Refresh();
    m_ra->Show();
    m_dec->Show();
    m_saveCSV->Hide();
    m_statusBar->SetStatusText(wxEmptyString);
    Layout(); // in case size changed
}

void AnalysisWin::OnClickFFT(wxCommandEvent& event)
{
    m_toggleDrift->SetValue(false);
    m_graph->Refresh();
    m_ra->Hide();
    m_dec->Hide();
    m_saveCSV->Show();
    m_statusBar->SetStatusText(wxEmptyString);
    Layout();
}

void AnalysisWin::OnSaveCSV(wxCommandEvent& event)
{
    if (m_garun.nfft == 0)
        return;

    wxFileDialog dlg(this, _("Save Frequency Analysis CSV"), wxEmptyString,
        wxString::Format("fft_%s.csv", m_garun.starts.Format("%Y%m%d_%H%M%S")),
        _("CSV files (*.csv)|*.csv|All files (*.*)|*.*"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK)
        return;

    wxFile f;
    if (!f.Open(dlg.GetPath(), wxFile::write))
    {
        wxMessageBox(_("Could not open file for writing."), _("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    f.Write("Period (s),Amplitude (px),Amplitude (\")\n");
    for (size_t i = 0; i < m_garun.nfft; i++)
    {
        f.Write(wxString::Format("%.4f,%.6f,%.6f\n",
            m_garun.fftx[i],
            m_garun.ffty[i],
            m_garun.ffty[i] * m_garun.pixscale));
    }
}

static void HZoom(AnalysisWin *aw, double f, int center)
{
    if (aw->m_toggleDrift->GetValue())
        s_drpos.HZoom(f, center);
    else
    {
        aw->m_cursor = -1; // fixme - re-position?
        s_fftpos.HZoom(f, center);
    }
    aw->m_graph->Refresh();
}

static void HZoomOut(AnalysisWin *aw, int center)
{
    HZoom(aw, 1.1, center);
}

static void HZoomIn(AnalysisWin *aw, int center)
{
    HZoom(aw, 1. / 1.1, center);
}

void AnalysisWin::OnMouseWheel(wxMouseEvent& evt)
{
    if (evt.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
    {
        if (evt.GetWheelRotation() > 0)
            HZoomIn(this, evt.GetPosition().x);
        else
            HZoomOut(this, evt.GetPosition().x);
    }
    evt.Skip();
}

void AnalysisWin::OnLeftDown(wxMouseEvent& event)
{
    s_drag.dragging = true;
    s_drag.drag_direction = DRAGDIR_UNKNOWN;
    s_drag.dragStart = s_drag.dragLast = event.GetPosition();
    if (m_toggleFFT->GetValue())
        s_drag.dragStartXofs = s_fftpos.xofs;
    else
        s_drag.dragStartXofs = s_drpos.xofs;
    m_graph->CaptureMouse();
    event.Skip();
}

void AnalysisWin::OnLeftUp(wxMouseEvent& event)
{
    s_drag.dragging = false;
    m_graph->ReleaseMouse();
}

void AnalysisWin::OnCaptureLost(wxMouseCaptureLostEvent& evt)
{
    s_drag.dragging = false;
    evt.Skip();
}

void AnalysisWin::OnMove(wxMouseEvent& event)
{
    if (s_drag.dragging)
    {
        const wxPoint& pos = event.GetPosition();
        int dx = pos.x - s_drag.dragLast.x;
        int dy = pos.y - s_drag.dragLast.y;
        s_drag.dragLast = pos;

        DragDirection prior_direction = s_drag.drag_direction;
        if (dx == 0)
        {
            s_drag.drag_direction = DRAGDIR_VERT;
        }
        else
        {
            s_drag.drag_direction = DRAGDIR_HORZ;
            dy = 0;
        }

        if (prior_direction != s_drag.drag_direction)
        {
            event.Skip();
            return;
        }

        if (dx != 0)
        {
            double xofs = s_drag.dragStartXofs + (double) (pos.x - s_drag.dragStart.x);
            if (m_toggleFFT->GetValue())
            {
                s_fftpos.xofs = xofs;
                m_cursor = -1;
            }
            else
            {
                s_drpos.xofs = xofs;
            }
        }
        if (dy != 0)
        {
            if (m_toggleFFT->GetValue())
                s_fftpos.VZoom(dy < 0 ? 1.05 : 1.0 / 1.05);
            else
                s_drpos.VZoom(dy < 0 ? 1.05 : 1.0 / 1.05);
        }

        m_graph->Refresh();
        return;
    }

    // not dragging

    int x = event.GetPosition().x;

    if (m_toggleFFT->GetValue())
    {
        if (x < s_fftpos.StartX() || x >= s_fftpos.EndX())
        {
            m_cursor = -1;
            return;
        }

        // find closest maximum within several pixels
        {
            enum { DIST = 8 };

            // look left
            int xl = x;
            bool foundl = false;
            for (int i = 0; i < DIST; i++, --xl)
            {
                if (xl - 1 < s_fftpos.StartX() || xl + 1 >= s_fftpos.EndX())
                    break;
                double a1 = s_fftpos.FEval(xl - 1);
                double a2 = s_fftpos.FEval(xl);
                double a3 = s_fftpos.FEval(xl + 1);
                if (a2 > a1 && a2 > a3)
                {
                    foundl = true;
                    break;
                }
            }

            int xr = x + 1;
            bool foundr = false;
            for (int i = 0; i < DIST; i++, ++xr)
            {
                if (xr - 1 < s_fftpos.StartX() || xr + 1 >= s_fftpos.EndX())
                    break;
                double a1 = s_fftpos.FEval(xr - 1);
                double a2 = s_fftpos.FEval(xr);
                double a3 = s_fftpos.FEval(xr + 1);
                if (a2 > a1 && a2 > a3)
                {
                    foundr = true;
                    break;
                }
            }

            if (foundl && foundr)
                m_cursor = x - xl < xr - x ? xl : xr;
            else if (foundl)
                m_cursor = xl;
            else if (foundr)
                m_cursor = xr;
            else
                m_cursor = x;
        }

        m_graph->Refresh();

        int y;
        double p, a;
        s_fftpos.Eval(m_cursor, &y, &p, &a);

        m_statusBar->SetStatusText(wxString::Format("Period: %.1fs  Amplitude: %.1f\" (%.2fpx)  P-P: %.1f\" (%.2fpx)  RMS: %.1f\" (%.2fpx)",
            p, a * m_garun.pixscale, a, 2. * a * m_garun.pixscale, 2. * a,
            M_SQRT2 / 2.0 * a * m_garun.pixscale, M_SQRT2 / 2.0 * a));
    }
    else
    {
        wxString s;
        double const t = s_drpos.T(x);
        if (m_garun.len >= 2 && t >= m_garun.t[0] && t <= m_garun.t[m_garun.len - 1])
        {
            int y = event.GetPosition().y;
            double yval = s_drpos.RaOrDec(y);
            s = wxString::Format("Time: %-.1fs  %s    Y: %.2f\" (%.2fpx)",
                    t, (m_garun.starts + wxTimeSpan(0, 0, t)).Format("%H:%M:%S"),
                    -yval * m_garun.pixscale, -yval);
        }
        m_statusBar->SetStatusText(s);
    }
}

static void PaintDrift(AnalysisWin *aw, const GARun& ga, wxDC& dc)
{
    const wxSize& sz = aw->m_graph->GetSize();

    // axes
    dc.SetPen(*wxGREY_PEN);

    // x-axis
    int const ymid = (s_drpos.y0 + s_drpos.y1) / 2;
    dc.DrawLine(s_drpos.x0, ymid, s_drpos.x1, ymid);

    // time divisions
    {
        enum { MINSEP = 40 };
        dc.SetTextForeground(*wxLIGHT_GREY);
#if defined(__WXOSX__)
        dc.SetFont(wxSMALL_FONT->Smaller());
#else
        dc.SetFont(wxSWISS_FONT->Smaller());
#endif
        dc.SetPen(wxPen(wxColour(80, 80, 80), 1, wxPENSTYLE_DOT));
        double const dt = s_drpos.T(MINSEP) - s_drpos.T(0);
        double const incr = pow(10.0, ceil(log10(dt)));
        double const start = ceil(s_drpos.T(s_drpos.x0) / incr) * incr;
        double const end = floor(s_drpos.T(s_drpos.x1) / incr) * incr;
        for (double t = start; t <= end; t += incr)
        {
            int x = s_drpos.X(t);
            dc.DrawLine(x, s_drpos.y0, x, s_drpos.y1);
            dc.DrawText(wxString::Format("%g", t), x + 2, s_drpos.y1 + 1);
        }
    }

    {
        // horizontal grid lines
        double vsc = s_drpos.scy;
        bool const arcsecs = wxGetApp().LVFrame()->ArcsecsSelected();
        if (arcsecs) // arc-seconds
            vsc /= ga.pixscale;
        double v = (double) sz.GetHeight() * (0.5 / 6.0) / vsc;
        double m = pow(10, ceil(log10(v)));
        double t;
        if (v < (t = .25 * m))
            v = t;
        else if (v < (t = .5 * m))
            v = t;
        else
            v = m;
        int iv = (int)(v * vsc);

        if (iv > 0)
        {
            int const y0 = (s_drpos.y0 + s_drpos.y1) / 2;
            int const x0 = s_drpos.x0;
            int const x1 = s_drpos.x1;
            dc.SetPen(wxPen(wxColour(60, 60, 60), 1, wxPENSTYLE_DOT));
            double dy = v;
            wxString format = arcsecs ? "%g\"" : "%g";
            for (int y = y0 - iv; y > s_drpos.y1; y -= iv, dy += v)
            {
                dc.DrawLine(x0, y, x1, y);
                dc.DrawText(wxString::Format(format, dy), 3, y + 2);
            }
            dy = -v;
            for (int y = y0 + iv; y < s_drpos.y0; y += iv, dy -= v)
            {
                dc.DrawLine(x0, y, x1, y);
                dc.DrawText(wxString::Format(format, dy), 3, y + 2);
            }
        }
    }

    size_t n = ga.len;
    bool plot_ra = aw->m_ra->GetValue(), plot_dec = aw->m_dec->GetValue();
    if (n < 2 || !(plot_ra || plot_dec))
        return;

    // find start/end indexes

    size_t i0 = 0;
    {
        double const tx0 = s_drpos.T(s_drpos.x0);
        if (tx0 > ga.t[0])
        {
            for (; i0 < n - 1; ++i0)
                if (ga.t[i0 + 1] > tx0)
                    break;
        }
    }

    size_t i1 = n - 1;
    {
        double const tx1 = s_drpos.T(s_drpos.x1);
        if (tx1 < ga.t[n - 1])
        {
            for (; i1 >= 1; --i1)
                if (ga.t[i1 - 1] < tx1)
                    break;
        }
    }

    if (i1 <= i0)
        return;

    s_tmp.alloc(i1 - i0 + 1);

    int np = 0;
    for (size_t i = i0; i <= i1; i++)
        s_tmp.pts[np++].x = (int) (s_drpos.xofs + ga.t[i] / s_drpos.scx);

    if (plot_ra)
    {
        np = 0;
        for (size_t i = i0; i <= i1; i++)
            s_tmp.pts[np++].y = ymid + (int) (ga.rac[i] * s_drpos.scy);
        dc.SetPen(wxPen(s_settings.raColor, 2));
        dc.DrawLines(np, s_tmp.pts);
    }

    if (plot_dec)
    {
        np = 0;
        for (size_t i = i0; i <= i1; i++)
            s_tmp.pts[np++].y = ymid - (int) (ga.decc[i] * s_drpos.scy);
        dc.SetPen(wxPen(s_settings.decColor, 2));
        dc.DrawLines(np, s_tmp.pts);
    }
}

static double IncrP(double p)
{
    return pow(10.0, floor(log10(p)));
}

static double StartP(double p)
{
    double incr = IncrP(p);
    return ceil(p / incr) * incr;
}

static void PaintFFT(AnalysisWin *aw, const GARun& ga, wxDC& dc)
{
    // x grid
    {
        dc.SetTextForeground(*wxLIGHT_GREY);
#if defined(__WXOSX__)
        dc.SetFont(wxSMALL_FONT->Smaller());
#else
        dc.SetFont(wxSWISS_FONT->Smaller());
#endif
        dc.SetPen(wxPen(wxColour(60, 60, 60), 1, wxPENSTYLE_SOLID));
        double p0 = s_fftpos.P(s_fftpos.x0);
        double p1 = s_fftpos.P(s_fftpos.x1);
        for (double p = StartP(p0); p < p1; p += IncrP(p))
        {
            int x = s_fftpos.X(p);
            dc.DrawLine(x, s_fftpos.y0, x, s_fftpos.y1);
            dc.DrawText(wxString::Format("%g", p), x + 2, s_fftpos.y1 + 1);
        }
        dc.DrawLine(s_fftpos.x0, s_fftpos.y0, s_fftpos.x1, s_fftpos.y0);
    }

    {
        // horizontal grid lines
        double vsc = s_fftpos.scy;
        bool const arcsecs = wxGetApp().LVFrame()->ArcsecsSelected();
        if (arcsecs) // arc-seconds
            vsc /= ga.pixscale;
        double v = (double) aw->m_graph->GetSize().GetHeight() * (0.5 / 6.0) / vsc;
        double m = pow(10, ceil(log10(v)));
        double t;
        if (v < (t = .25 * m))
            v = t;
        else if (v < (t = .5 * m))
            v = t;
        else
            v = m;
        int iv = (int) (v * vsc);

        if (iv > 0)
        {
            int const y0 = s_drpos.y0;
            int const x0 = s_drpos.x0;
            int const x1 = s_drpos.x1;
            dc.SetPen(wxPen(wxColour(60, 60, 60), 1, wxPENSTYLE_DOT));
            double dy = v;
            wxString format = arcsecs ? "%g\"" : "%g";
            for (int y = y0 - iv; y > s_drpos.y1; y -= iv, dy += v)
            {
                dc.DrawLine(x0, y, x1, y);
                dc.DrawText(wxString::Format(format, dy), 3, y + 2);
            }
        }
    }

    int const dx = 1;
    int nx = (s_fftpos.x1 - s_fftpos.x0 + dx - 1) / dx + 1;
    s_tmp.alloc(nx);

    int i = 0;
    for (int x = s_fftpos.StartX(); x < s_fftpos.EndX(); x += dx, ++i)
    {
        s_tmp.pts[i].x = x;
        s_tmp.pts[i].y = s_fftpos.Eval(x);
    }

    dc.SetPen(wxPen(s_settings.raColor, 2));
    dc.DrawLines(i, s_tmp.pts);

    if (aw->m_cursor >= 0)
    {
        wxPen YellowDashPen(wxColour(140, 140, 0), 1, wxPENSTYLE_DOT);
        dc.SetPen(YellowDashPen);
        dc.DrawLine(aw->m_cursor, s_fftpos.y0, aw->m_cursor, s_fftpos.y1);
        int y = s_fftpos.Eval(aw->m_cursor);
        dc.SetBrush(*wxWHITE);
        dc.DrawCircle(aw->m_cursor, y, 4);
    }
}

void AnalysisWin::OnPaintGraph(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(m_graph);
    dc.Clear();
    if (m_toggleDrift->GetValue())
        PaintDrift(this, m_garun, dc);
    else
        PaintFFT(this, m_garun, dc);
}

void AnalysisWin::OnHMinus(wxCommandEvent& event)
{
    HZoomOut(this, -1);
}

void AnalysisWin::OnHPlus(wxCommandEvent& event)
{
    HZoomIn(this, -1);
}

void AnalysisWin::OnHReset(wxCommandEvent& event)
{
    if (m_toggleFFT->GetValue())
    {
        s_fftpos.HReset(m_graph->GetSize().x);
        m_cursor = -1; // fixme - re-position?
    }
    else
        s_drpos.HReset(m_graph->GetSize().x);

    m_graph->Refresh();
}

void AnalysisWin::OnVMinus(wxCommandEvent& event)
{
    if (m_toggleFFT->GetValue())
        s_fftpos.VZoom(1.0 / 1.1);
    else
        s_drpos.VZoom(1.0 / 1.1);

    m_graph->Refresh();
}

void AnalysisWin::OnVPlus(wxCommandEvent& event)
{
    if (m_toggleFFT->GetValue())
        s_fftpos.VZoom(1.1);
    else
        s_drpos.VZoom(1.1);

    m_graph->Refresh();
}

void AnalysisWin::OnVReset(wxCommandEvent& event)
{
    if (m_toggleFFT->GetValue())
        s_fftpos.VReset(m_graph->GetSize().y);
    else
        s_drpos.VReset(m_graph->GetSize().y);

    m_graph->Refresh();
}
