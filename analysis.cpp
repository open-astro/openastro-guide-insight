/*
 * This file is part of phdlogview
 *
 * Copyright (C) 2018 Andy Galasso
 * Copyright (C) 2026 Open Astro contributors
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

#include "analysis.h"

#include "logparser.h"

#include <algorithm>
#include <charconv>
#include <math.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_spline.h>

void Spline::Init(const double *x, const double *y, size_t n)
{
    if (accel)
        gsl_interp_accel_reset(static_cast<gsl_interp_accel *>(accel));
    else
        accel = gsl_interp_accel_alloc();

    if (spline)
        gsl_spline_free(static_cast<gsl_spline *>(spline));

    spline = gsl_spline_alloc(gsl_interp_akima, n);

    gsl_spline_init(static_cast<gsl_spline *>(spline), x, y, n);
}

Spline::~Spline()
{
    gsl_spline_free(static_cast<gsl_spline *>(spline));
    gsl_interp_accel_free(static_cast<gsl_interp_accel *>(accel));
}

double Spline::Eval(double x) const
{
    return gsl_spline_eval(static_cast<gsl_spline *>(spline),
        x, static_cast<gsl_interp_accel *>(accel));
}

struct LFit
{
    double avx, avy, varx, covxy, vary, n;
    LFit() : avx(0.), avy(0.), varx(0.), covxy(0.), vary(0.), n(0.) { }
    void data(double x, double y) {
        double k = n;
        n += 1.0;
        k /= n;
        double dx = x - avx;
        double dy = y - avy;
        varx += (k * dx * dx - varx) / n;
        covxy += (k * dx * dy - covxy) / n;
        vary += (k * dy * dy - vary) / n;
        avx += dx / n;
        avy += dy / n;
    }
    void reset() {
        avx = avy = varx = covxy = vary = n = 0.;
    }
    // y = a + b x
    double B() const { return n >= 2. ? covxy / varx : 0.; }
    double A() const { return avy - B() * avx; }
    void result(double *a, double *b) const {
        *b = B();
        *a = avy - *b * avx;
    }
    double Theta() const { return n >= 2. ? atan2(covxy, varx) : 0.; }
};

inline static bool Include(const GuideEntry& e)
{
    return e.included && StarWasFound(e.err);
}

static double DecDrift(const GuideSession::EntryVec& entries)
{
    if (entries.size() < 2)
        return 0.;

    auto it = entries.begin();
    for (; it != entries.end(); ++it)
        if (Include(*it))
            break;
    if (it == entries.end())
        return 0.;

    double y_accum = 0.;
    double prev_y = it->decraw;
    bool prev_guided = it->decdur != 0;

    LFit fit;

    fit.data(it->dt, y_accum);
    ++it;

    for (; it != entries.end(); ++it)
    {
        if (!Include(*it))
            continue;

        double y = it->decraw;
        if (!prev_guided)
        {
            double dy = y - prev_y;
            y_accum += dy;
            fit.data(it->dt, y_accum);
        }
        prev_y = y;
        prev_guided = it->decdur != 0;
    }

    return fit.B();
}

static double RaDrift(const GuideSession::EntryVec& entries)
{
    // estimate RA drift = (RA offset + sum of RA corrections) / time

    bool found = false;
    double ra0 = 0., t0 = 0.;
    auto it = entries.begin();
    for (; it != entries.end(); ++it)
    {
        if (Include(*it))
        {
            ra0 = it->raraw;
            t0 = it->dt;
            found = true;
            break;
        }
    }

    if (!found)
        return 0.;

    double sum = 0.;
    for (; it != entries.end(); ++it)
    {
        // dropped frames may have ra corrections
        if (it->included)
            sum += it->radur ? it->raguide : 0.;
    }

    // the forward search found an included entry, so this search finds one too
    double ra1 = ra0, t1 = t0;
    for (auto itr = entries.rbegin(); itr != entries.rend(); ++itr)
    {
        if (Include(*itr))
        {
            ra1 = itr->raraw;
            t1 = itr->dt;
            break;
        }
    }

    return t1 > t0 ? (ra1 - ra0 - sum) / (t1 - t0) : 0.;
}

static double PolarAlignError(const GuideSession& session)
{
    // polar alignment error from Barrett:
    // http://celestialwonders.com/articles/polaralignment/PolarAlignmentAccuracy.pdf
    return 3.8197 * fabs(session.drift_dec) * session.pixelScale / cos(session.declination);
}

void GuideSession::CalcStats()
{
    LFit fitrd;
    double peak_r = 0., peak_d = 0.;

    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        const GuideEntry& e = *it;
        if (!Include(e))
            continue;

        fitrd.data(e.raraw, e.decraw);

        if (fabs(e.raraw) > fabs(peak_r))
            peak_r = e.raraw;
        if (fabs(e.decraw) > fabs(peak_d))
            peak_d = e.decraw;
    }

    rms_ra = sqrt(fitrd.varx);
    rms_dec = sqrt(fitrd.vary);
    avg_ra = fitrd.avx;
    avg_dec = fitrd.avy;
    peak_ra = peak_r;
    peak_dec = peak_d;

    // angle of elongation
    theta = fitrd.Theta();

    // now get variances of the transformed coordinates offset by the
    // mean and rotated by theta
    double cost = cos(theta), sint = sin(theta);

    LFit fitxy;
    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        const GuideEntry& e = *it;
        if (!Include(e))
            continue;

        double dr = e.raraw - avg_ra;
        double dd = e.decraw - avg_dec;
        double x = dr * cost + dd * sint;
        double y = dd * cost - dr * sint;

        fitxy.data(x, y);
    }

    lx = sqrt(fitxy.varx);
    ly = sqrt(fitxy.vary);

    {
        double a = lx, b = ly;
        if (a < b)
            std::swap(a, b);

        elongation = (a + b) > 1e-6 ?
                (a - b) / (a + b) :
                1.;
    }

    drift_ra = RaDrift(entries) * 60.;   // pixels per minute
    drift_dec = DecDrift(entries) * 60.;
    paerr = PolarAlignError(*this);
}

GARun::~GARun()
{
    delete[] t;
    delete[] rac;
    delete[] decc;
    delete[] fftx;
    delete[] ffty;
}

struct Line
{
    double a, b;
    Line(const LFit& lf) {
        lf.result(&a, &b);
    }
    double operator()(double x) { return a + b * x; }
};

bool GARun::CanAnalyze(const GuideSession& session, size_t begin, size_t end)
{
    const auto& entries = session.entries;
    const auto& p0 = entries.begin() + begin;
    const auto& p1 = entries.begin() + end;

    enum { MIN_ENTRIES = 12 }; // need at least 12 for FFT output spline (N / 2 - 1 >= 5)

    size_t n = 0;
    for (auto it = p0; it != p1; ++it)
        if (Include(*it) && ++n >= MIN_ENTRIES)
            return true;

    return false;
}

void GARun::Analyze(const GuideSession& session, size_t begin, size_t end, bool undo_ra_corrections)
{
    starts = session.starts;
    pixscale = session.pixelScale;

    const auto& entries = session.entries;
    const auto& p0 = entries.begin() + begin;
    const auto& p1 = entries.begin() + end;

    size_t n = std::count_if(p0, p1, &Include);

    delete[] t;
    delete[] rac;
    delete[] decc;
    delete[] fftx;
    delete[] ffty;

    len = n;
    t = new double[n];
    double *ra = new double[n];
    double *dec = new double[n];
    rac = new double[n];
    decc = new double[n];

    LFit fitR; // ra fit
    LFit fitD; // dec fit

    double *pt = &t[0];
    double *pra = &ra[0];
    double *pdec = &dec[0];

    {
        double rapos = 0.;
        double prev_raguide = 0.;
        double prev_raraw = 0.;

        for (auto it = p0; it != p1; ++it)
        {
            const GuideEntry& e = *it;
            if (Include(e))
            {
                double const raraw = e.raraw;
                double const raguide = e.raguide;
                double const move = raraw - prev_raraw - prev_raguide;
                rapos += move;
                prev_raraw = raraw;
                prev_raguide = undo_ra_corrections ? raguide : 0.;

                *pt++ = e.dt;
                *pra++ = rapos;
                *pdec++ = e.decraw;

                fitR.data(e.dt, rapos);
                fitD.data(e.dt, e.decraw);
            }
        }
    }

    // drift correction

    Line lR(fitR);
    Line lD(fitD);

    pt = &t[0];
    pra = &ra[0];
    pdec = &dec[0];
    double *prac = &rac[0];
    double *pdecc = &decc[0];
    while (prac < &rac[n])
    {
        double tt = *pt++;
        *prac++ = *pra++ - lR(tt);
        *pdecc++ = *pdec++ - lD(tt);
    }

    // interpolate RA to get uniform samples for FFT

    double *data = new double[n * 2];

    double dt = (t[n - 1] - t[0]) / (double) (n - 1);

    {
        double const k = M_PI * 2.0 / (double) (n - 1);
        Spline spline(t, rac, n);
        double x = t[0];
        for (unsigned int i = 0; i < n; i++, x += dt)
        {
            if (x > t[n - 1]) x = t[n - 1]; // rounding error can put the last point over the boundary
            // Hamming window
            double const hw = 0.54 - 0.46 * cos(i * k);
            data[i * 2] = hw * spline.Eval(x);
            data[i * 2 + 1] = 0.;
        }
    }

    // FFT

    {
        gsl_fft_complex_workspace *work = gsl_fft_complex_workspace_alloc(n);
        gsl_fft_complex_wavetable *wt = gsl_fft_complex_wavetable_alloc(n);

        gsl_fft_complex_forward(data, 1, n, wt, work);

        gsl_fft_complex_wavetable_free(wt);
        gsl_fft_complex_workspace_free(work);

        nfft = n / 2 - 1; // omit steady state f=0
        fftx = new double[nfft];
        ffty = new double[nfft];

        double scale = 4. / (double) n; // http://www.stat.ucla.edu/~frederic/221/W17/221ch4a.pdf
        fftymax = 0.;

        for (size_t i = 0; i < nfft; i++)
        {
            const gsl_complex *pz = (const gsl_complex *) (&data[(i + 1) * 2]);
            double f = (double) (i + 1) / ((double) n * dt);
            double p = 1. / f;
            fftx[nfft - 1 - i] = p;
            double a = gsl_complex_abs(*pz) * scale;
            ffty[nfft - 1 - i] = a;
            if (a > fftymax)
                fftymax = a;
        }

        ffts.Init(fftx, ffty, nfft);
    }

    delete[] data;
    delete[] ra;
    delete[] dec;
}


// Fixed-point text with a decimal point regardless of the C locale.
static void AppendFixed(std::string& out, double v, int precision)
{
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed, precision);
    out.append(buf, res.ptr);
}

std::string FftCsv(const GARun& ga)
{
    std::string out("Period (s),Amplitude (px),Amplitude (\")\n");
    for (size_t i = 0; i < ga.nfft; i++)
    {
        AppendFixed(out, ga.fftx[i], 4);
        out += ',';
        AppendFixed(out, ga.ffty[i], 6);
        out += ',';
        AppendFixed(out, ga.ffty[i] * ga.pixscale, 6);
        out += '\n';
    }
    return out;
}

std::string SessionLabel(const GuideSession& session)
{
    bool ga = false;
    for (const InfoEntry& info : session.infos)
        if (info.info.rfind("GA Result", 0) == 0)
            ga = true;

    bool anyGuided = false, anyUnguided = false;
    for (const GuideEntry& e : session.entries)
        (e.guiding ? anyGuided : anyUnguided) = true;

    if (ga)
        return anyGuided ? "Guiding + GA" : "Guiding Assistant";

    if (anyUnguided && !anyGuided)
        return "Unguided";

    for (const wxString& line : session.hdr)
        if (line.Contains("DEC guide mode = Off"))
            return "Guiding, RA only";

    return "Guiding";
}
