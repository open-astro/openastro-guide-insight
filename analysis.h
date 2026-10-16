/*
 * This file is part of phdlogview
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
    void Analyze(const GuideSession& session, size_t begin, size_t end, bool undo_ra_corrections);
};

// The periodogram as CSV text: a header row, then period (s), amplitude (px)
// and amplitude (arc-seconds) per row.
std::string FftCsv(const GARun& ga);

// What kind of run a guiding section was, for the log's table of contents:
// "Guiding", "Guiding Assistant", "Guiding + GA", "Unguided" or
// "Guiding, RA only".
std::string SessionLabel(const GuideSession& session);

#endif
