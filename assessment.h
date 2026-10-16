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

#ifndef ASSESSMENT_INCLUDED
#define ASSESSMENT_INCLUDED

// How good a stretch of guiding was and what to change, in plain words,
// independent of the GUI so it can be tested.

#include <wx/arrstr.h>

#include "analysis.h"
#include "graphmath.h"
#include "logparser.h"

#include <optional>
#include <string>
#include <vector>

// The telescope and camera taking the images, from the "Imaging telescope ="
// and "Imaging camera =" header lines OpenAstro guiders write. Stock PHD2
// logs have neither.
struct ImagingTrain
{
    std::string telescope;
    std::string camera;
    std::optional<double> focalLength; // mm
    std::optional<double> pixelScale;  // arc-seconds per imaging pixel
};
ImagingTrain ImagingTrainFrom(const wxArrayString& hdr);

// What a Guiding Assistant run measured with guiding off: the star's
// high-frequency motion, which is what seeing and the measurement leave
// for guiding to work against.
struct SkyFloor
{
    double total;       // arc-seconds, RA and Dec combined
    bool sameSession;   // measured in the session being assessed
};
// The floor from the session's own Guiding Assistant result, or else the
// latest one earlier in the log.
std::optional<SkyFloor> SkyFloorFor(const GuideLog& log, const GuideSession& session);

// The numbers the assessment rests on, over the guided frames that count
// (included, star found, guide output on) among entries [begin, end).
struct Measurements
{
    size_t frames = 0;
    size_t lost = 0;            // frames where the star was not found
    double medianSnr = 0.;
    double hfd = 0.;            // px, from the header; 0 when not logged
    double raLag1 = 0.;         // correlation of each frame's RA error with the next
    double decLag1 = 0.;
    double raPulsed = 0.;       // fraction of frames with an RA pulse
    double decPulsed = 0.;
    size_t north = 0, south = 0; // Dec pulses each way
    size_t maxedPulses = 0;     // pulses at the longest duration allowed
    double raAggression = 0.;   // from the header; 0 when not logged
    double raMinMove = 0., decMinMove = 0.; // px
    double exposure = 0.;       // seconds
    double orthoError = -1.;    // degrees; negative when not logged
    double calDec = NAN, dec = NAN; // degrees
    // 5-minute RMS (arc-seconds) at its best and worst, and the frames of the
    // worst 5 minutes; no worst window when the stretch is under 10 minutes.
    double bestWindow = 0., worstWindow = 0.;
    std::optional<IndexRun> worst;
    size_t dithers = 0, unsettled = 0;
    size_t worstDithers = 0;    // dithers during the worst window
    // From "INFO: CONDITIONS" lines during the worst window, when any.
    std::optional<double> worstWind;   // m/s, the highest gust or speed
};
Measurements Measure(const GuideSession& session, size_t begin, size_t end);

enum class Verdict { Excellent, Good, Fair, Poor, Unknown };

// A suggested setting change.
struct Change
{
    std::string setting;
    std::string from;
    std::string to;
};

struct Finding
{
    std::string title;
    std::string body;
    std::string evidence;          // the numbers behind it, in small print
    std::vector<Change> changes;
    std::optional<IndexRun> frames; // frames to show on the timeline
};

struct Assessment
{
    Verdict verdict = Verdict::Unknown;
    std::string verdictLabel;  // "Excellent", "Good", ... or empty
    std::string total;         // total RMS, "2.08″"
    std::string scale;         // the total against the imaging pixels
    double ratio = 0.;         // total RMS / imaging pixel scale, 0 when unknown
    std::string why;           // why the total is what it is
    std::vector<Finding> improve;
    std::vector<Finding> good;
};

Assessment Assess(const GuideLog& log, const GuideSession& session, size_t begin, size_t end,
                  const GuideStats& stats);

#endif
