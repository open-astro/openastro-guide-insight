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

#include "labels.h"

#include <algorithm>
#include <charconv>
#include <math.h>

static const char *const MINUS = "−";
static const char *const ARCSEC = "″";
static const char *const DOT = " · ";

// v with a decimal point whatever the locale, and an ASCII minus sign.
static std::string Plain(double v, int decimals)
{
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed, decimals);
    std::string s(buf, res.ptr);
    // a value that rounds to zero has no sign
    if (s[0] == '-' && s.find_first_not_of("-0.") == std::string::npos)
        s.erase(0, 1);
    return s;
}

std::string Fixed(double v, int decimals)
{
    std::string s = Plain(v, decimals);
    if (s[0] == '-')
        s.replace(0, 1, MINUS);
    return s;
}

std::string Signed(double v, int decimals)
{
    std::string s = Fixed(v, decimals);
    return s.rfind(MINUS, 0) == 0 ? s : "+" + s;
}

std::string Duration(double seconds)
{
    long long n = llround(std::max(seconds, 0.));
    if (n < 60)
        return std::to_string(n) + " s";

    long long big, small;
    const char *bigUnit, *smallUnit;
    if (n < 3600)
    {
        big = n / 60, small = n % 60;
        bigUnit = " min", smallUnit = " s";
    }
    else
    {
        big = n / 3600, small = n % 3600 / 60;
        bigUnit = " h", smallUnit = " min";
    }
    std::string s = std::to_string(big) + bigUnit;
    if (small)
        s += " " + std::to_string(small) + smallUnit;
    return s;
}

std::string Distance(double pixels, double pixelScale, Units units, int decimals)
{
    if (units == Units::ArcSeconds)
        return Fixed(pixels * pixelScale, decimals) + ARCSEC;
    return Fixed(pixels, decimals) + " px";
}

static std::string ClockTime(const LogSection& section, const char *format = "%H:%M")
{
    if (!section.starts.IsValid())
        return std::string(section.date.utf8_str());
    return section.starts.Format(format).ToStdString();
}

SectionRow RowFor(const GuideLog& log, size_t section, Units units)
{
    const LogSectionLoc& loc = log.sections[section];
    SectionRow row;
    if (loc.type == CALIBRATION_SECTION)
    {
        const Calibration& cal = log.calibrations[loc.idx];
        row.calibration = true;
        row.kind = "Calibration";
        row.meta = ClockTime(cal) + DOT + std::to_string(cal.entries.size()) + " steps";
    }
    else
    {
        const GuideSession& s = log.sessions[loc.idx];
        row.calibration = false;
        row.kind = SessionLabel(s);
        row.meta = ClockTime(s) + DOT + Duration(s.duration);
        row.rms = Distance(hypot(s.rms_ra, s.rms_dec), s.pixelScale, units);
    }
    return row;
}

std::string SectionTitle(const GuideLog& log, size_t section)
{
    const LogSectionLoc& loc = log.sections[section];
    if (loc.type == CALIBRATION_SECTION)
        return "Calibration" + std::string(DOT) + ClockTime(log.calibrations[loc.idx], "%H:%M:%S");
    const GuideSession& s = log.sessions[loc.idx];
    return SessionLabel(s) + DOT + ClockTime(s, "%H:%M:%S");
}

std::string DayHeading(const wxDateTime& when)
{
    return when.Format("%A, %B ").ToStdString() + std::to_string(when.GetDay()) + ", " +
        std::to_string(when.GetYear());
}

double TotalGuiding(const GuideLog& log)
{
    double total = 0.;
    for (const auto& s : log.sessions)
        total += s.duration;
    return total;
}

static std::string Drift(double pixelsPerMinute, double pixelScale, Units units)
{
    if (units == Units::ArcSeconds)
        return Signed(pixelsPerMinute * pixelScale, 2) + ARCSEC + "/min";
    return Signed(pixelsPerMinute, 2) + " px/min";
}

static std::string Frames(size_t n)
{
    return std::to_string(n) + (n == 1 ? " frame" : " frames");
}

SummaryText Summarize(const GuideStats& stats, size_t frames, double pixelScale, Units units)
{
    Units other = units == Units::ArcSeconds ? Units::Pixels : Units::ArcSeconds;
    double total = hypot(stats.rms_ra, stats.rms_dec);

    SummaryText s;
    s.total = Distance(total, pixelScale, units);
    s.alt = Distance(total, pixelScale, other);
    s.ra = Distance(stats.rms_ra, pixelScale, units);
    s.dec = Distance(stats.rms_dec, pixelScale, units);
    s.peak = "RA " + Distance(stats.peak_ra, pixelScale, units) + DOT + "Dec " +
        Distance(stats.peak_dec, pixelScale, units);
    s.elongation = Fixed(stats.elongation * 100., 0) + "%";
    s.frames = stats.frames == frames ? Frames(frames) : std::to_string(stats.frames) + " of " + Frames(frames);
    s.raDrift = Drift(stats.drift_ra, pixelScale, units);
    s.decDrift = Drift(stats.drift_dec, pixelScale, units);
    s.paError = Fixed(stats.paerr, 1) + "′";
    return s;
}

std::string StatsCsv(const GuideStats& stats, double pixelScale)
{
    auto row = [pixelScale](const char *name, double px) {
        return std::string(name) + "," + Plain(px * pixelScale, 2) + "," + Plain(px, 2) + "\n";
    };
    return std::string("Statistic,arc-sec,pixels\n") +
        row("RA RMS", stats.rms_ra) +
        row("Dec RMS", stats.rms_dec) +
        row("Total RMS", hypot(stats.rms_ra, stats.rms_dec)) +
        row("RA peak", stats.peak_ra) +
        row("Dec peak", stats.peak_dec) +
        "Elongation (%)," + Plain(stats.elongation * 100., 0) + ",\n" +
        row("RA drift (per min)", stats.drift_ra) +
        row("Dec drift (per min)", stats.drift_dec) +
        "Polar alignment error (arc-min)," + Plain(stats.paerr, 1) + ",\n";
}
