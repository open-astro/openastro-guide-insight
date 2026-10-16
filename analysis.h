/*
 * This file is part of OpenAstro Guide Insight, a fork of PHD2 Log Viewer
 *
 * Copyright (C) 2016-2018 Andy Galasso
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

#ifndef ANALYSIS_INCLUDED
#define ANALYSIS_INCLUDED

// Guiding statistics and Guiding Assistant analysis, independent of the GUI.

#include <wx/datetime.h>

#include <stddef.h>
#include <string>
#include <vector>

#include "graphmath.h"

struct GuideSession;

class Spline
{
    void *spline;
    void *accel;
public:
    Spline() : spline(nullptr), accel(nullptr) { }
    Spline(const double *x, const double *y, size_t n) : spline(nullptr), accel(nullptr) { Init(x, y, n); }
    void Init(const double *x, const double *y, size_t n);
    ~Spline();
    double Eval(double x) const;
};

struct GARun
{
    wxDateTime starts;
    double pixscale;
    size_t len;
    double *t;
    double *rac;  // drift-corrected RA
    double *decc; // drift-corrected Dec
    size_t nfft;
    double *fftx; // FFT period
    double *ffty; // FFT amplitude
    Spline ffts;  // FFT spline for graphing
    double fftymax;
    GARun() : len(0), t(nullptr), rac(nullptr), decc(nullptr), nfft(0), fftx(nullptr), ffty(nullptr) { }
    ~GARun();
    static bool CanAnalyze(const GuideSession& session, size_t begin, size_t end);
    // drift_correction removes a straight-line fit from RA and Dec before the
    // drift plot and periodogram; turning it off shows the raw positions.
    void Analyze(const GuideSession& session, size_t begin, size_t end, bool undo_ra_corrections,
                 bool drift_correction = true);
};

// Statistics over the included frames, where the star was found, of entries
// [begin, end) of a session.
struct GuideStats
{
    size_t frames = 0;  // frames counted
    double rms_ra = 0., rms_dec = 0.;
    double avg_ra = 0., avg_dec = 0.;
    double theta = 0.;  // angle of the elongation ellipse's x-axis
    double lx = 0., ly = 0.; // ellipse axes
    double elongation = 0.;
    double peak_ra = 0., peak_dec = 0.;
    double drift_ra = 0., drift_dec = 0.; // pixels per minute
    double paerr = 0.;  // polar alignment error, arc-minutes
};
GuideStats CalcStats(const GuideSession& session, size_t begin, size_t end);

// The periodogram as CSV text: a header row, then period (s), amplitude (px)
// and amplitude (arc-seconds) per row.
std::string FftCsv(const GARun& ga);

// The stretches of a session with guide output off, such as a Guiding
// Assistant run, that have enough frames to analyse.
std::vector<IndexRun> GuidingAssistantRuns(const GuideSession& session);

// What kind of run a guiding section was, for the log's table of contents:
// "Guiding", "Guiding Assistant", "Guiding + GA", "Unguided" or
// "Guiding, RA only".
std::string SessionLabel(const GuideSession& session);

#endif
