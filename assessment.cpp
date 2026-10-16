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

#include "assessment.h"

#include "labels.h"
#include "timeline.h"

#include <algorithm>
#include <cctype>
#include <math.h>

namespace {

const char *const ARCSEC = "\u2033";
const char *const ARCMIN = "\u2032";

bool StartsWith(const std::string& s, const std::string& prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}

// The first comma-separated item of a header value.
std::string FirstItem(const std::string& s)
{
    return s.substr(0, s.find(", "));
}

// The value of "key = value" anywhere in the line, up to the next comma.
std::string Setting(const std::string& line, const std::string& key)
{
    std::string k = key + " = ";
    size_t pos = line.find(k);
    if (pos == std::string::npos)
        return std::string();
    return FirstItem(line.substr(pos + k.size()));
}

// The number a piece of text starts with, after any spaces: 3.76 from
// "3.76 x 3.76 um", 2 from "2x2". (std::from_chars for doubles is missing
// from the C++ library of the macOS build machines.)
std::optional<double> Leading(const std::string& text)
{
    size_t start = text.find_first_not_of(' ');
    if (start == std::string::npos)
        return std::nullopt;
    size_t end = start;
    while (end < text.size() && (isdigit((unsigned char) text[end]) || text[end] == '.' || text[end] == '-' ||
                                 text[end] == '+'))
        end++;
    double v;
    if (end == start || !wxString::FromUTF8(text.substr(start, end - start)).ToCDouble(&v))
        return std::nullopt;
    return v;
}

std::optional<double> Positive(const std::string& text)
{
    std::optional<double> v = Leading(text);
    return v && *v > 0. ? v : std::nullopt;
}

std::string Line(const wxString& s)
{
    return std::string(s.utf8_str());
}

std::string Percent(double fraction)
{
    return Fixed(fraction * 100., 0) + "%";
}

std::string Arcsec(double v)
{
    return Fixed(v, 2) + ARCSEC;
}

// Rounds up to a multiple of 0.05.
double UpToTwentieth(double v)
{
    return ceil(v / 0.05 - 1e-9) * 0.05;
}

double ToTwentieth(double v)
{
    return round(v / 0.05) * 0.05;
}

std::string Seconds(double s)
{
    return Fixed(s, s == floor(s) ? 0 : 1) + " s";
}

double Correlation(const std::vector<std::pair<double, double>>& pairs)
{
    if (pairs.size() < 3)
        return 0.;
    double mx = 0., my = 0.;
    for (const auto& [x, y] : pairs)
    {
        mx += x;
        my += y;
    }
    mx /= pairs.size();
    my /= pairs.size();
    double sxy = 0., sxx = 0., syy = 0.;
    for (const auto& [x, y] : pairs)
    {
        sxy += (x - mx) * (y - my);
        sxx += (x - mx) * (x - mx);
        syy += (y - my) * (y - my);
    }
    return sxx > 0. && syy > 0. ? sxy / sqrt(sxx * syy) : 0.;
}

bool Counted(const GuideEntry& e)
{
    return e.included && e.guiding && StarWasFound(e.err);
}

// The "Total HPF-RMS" of a Guiding Assistant result line, in arc-seconds.
std::optional<double> TotalHpfRms(const std::string& info, double pixelScale)
{
    if (!StartsWith(info, "GA Result"))
        return std::nullopt;
    const std::string key = "Total HPF-RMS=";
    size_t pos = info.find(key);
    if (pos == std::string::npos)
        return std::nullopt;
    std::string value = info.substr(pos + key.size());
    size_t open = value.find('(');
    if (open != std::string::npos)
        if (std::optional<double> arcsec = Positive(value.substr(open + 1)))
            return arcsec;
    if (std::optional<double> px = Positive(value))
        return *px * pixelScale;
    return std::nullopt;
}

std::optional<double> LastFloor(const GuideSession& s)
{
    std::optional<double> floor;
    for (const InfoEntry& info : s.infos)
        if (std::optional<double> v = TotalHpfRms(info.info, s.pixelScale))
            floor = v;
    return floor;
}

} // namespace

ImagingTrain ImagingTrainFrom(const wxArrayString& hdr)
{
    ImagingTrain t;
    std::optional<double> pixelSize, binning;
    for (const wxString& wxln : hdr)
    {
        std::string line = Line(wxln);
        if (StartsWith(line, "Imaging telescope = "))
        {
            t.telescope = FirstItem(line.substr(20));
            t.focalLength = Positive(Setting(line, "Focal length"));
        }
        else if (StartsWith(line, "Imaging camera = "))
        {
            std::string name = FirstItem(line.substr(17));
            if (StartsWith(name, "ambiguous") || StartsWith(name, "unknown"))
                continue;
            t.camera = name;
            t.pixelScale = Positive(Setting(line, "Pixel scale"));
            pixelSize = Positive(Setting(line, "Pixel size"));
            binning = Positive(Setting(line, "Binning"));
        }
    }
    if (!t.pixelScale && pixelSize && t.focalLength)
        t.pixelScale = 206.265 * *pixelSize * binning.value_or(1.) / *t.focalLength;
    return t;
}

std::optional<SkyFloor> SkyFloorFor(const GuideLog& log, const GuideSession& session)
{
    if (std::optional<double> own = LastFloor(session))
        return SkyFloor{ *own, true };

    // the latest result in an earlier session
    for (size_t i = 0; i < log.sessions.size(); i++)
    {
        if (&log.sessions[i] != &session)
            continue;
        for (size_t j = i; j-- > 0;)
            if (std::optional<double> earlier = LastFloor(log.sessions[j]))
                return SkyFloor{ *earlier, false };
        break;
    }
    return std::nullopt;
}

Measurements Measure(const GuideSession& session, size_t begin, size_t end)
{
    Measurements m;
    const auto& entries = session.entries;
    end = std::min(end, entries.size());

    std::vector<double> snr;
    std::vector<std::pair<double, double>> ra, dec;
    size_t raPulses = 0, decPulses = 0;
    for (size_t i = begin; i < end; i++)
    {
        const GuideEntry& e = entries[i];
        if (e.included && e.guiding && !StarWasFound(e.err))
            m.lost++;
        if (!Counted(e))
            continue;
        m.frames++;
        snr.push_back(e.snr);
        if (i + 1 < end && Counted(entries[i + 1]))
        {
            ra.emplace_back(e.raraw, entries[i + 1].raraw);
            dec.emplace_back(e.decraw, entries[i + 1].decraw);
        }
        if (e.mount != MOUNT)
            continue;
        if (e.radur)
            raPulses++;
        if (e.decdur)
        {
            decPulses++;
            (e.decdur > 0 ? m.north : m.south)++;
        }
        const Mount& mount = session.mount;
        if ((mount.xlim.maxDur > 0. && abs(e.radur) >= mount.xlim.maxDur) ||
            (mount.ylim.maxDur > 0. && abs(e.decdur) >= mount.ylim.maxDur))
            m.maxedPulses++;
    }
    m.raLag1 = Correlation(ra);
    m.decLag1 = Correlation(dec);
    if (m.frames)
    {
        m.raPulsed = (double) raPulses / m.frames;
        m.decPulsed = (double) decPulses / m.frames;
        std::nth_element(snr.begin(), snr.begin() + snr.size() / 2, snr.end());
        m.medianSnr = snr[snr.size() / 2];
    }

    m.raMinMove = session.mount.xlim.minMo;
    m.decMinMove = session.mount.ylim.minMo;
    for (const wxString& wxln : session.hdr)
    {
        std::string line = Line(wxln);
        if (StartsWith(line, "Exposure = "))
            m.exposure = Positive(line.substr(11)).value_or(0.) / 1000.;
        else if (StartsWith(line, "X guide algorithm = "))
            m.raAggression = Positive(Setting(line, "Aggression")).value_or(0.);
        else if (StartsWith(line, "Lock position = "))
            m.hfd = Positive(Setting(line, "HFD")).value_or(0.);
        else if (line.find("ortho.err. = ") != std::string::npos)
            m.orthoError = Leading(Setting(line, "ortho.err.")).value_or(-1.);
        else if (StartsWith(line, "RA Guide Speed = ") || line.find("Cal Dec = ") != std::string::npos)
            m.calDec = Leading(Setting(line, "Cal Dec")).value_or(NAN);
        if (StartsWith(line, "RA = "))
            m.dec = Leading(Setting(line, "Dec")).value_or(NAN);
    }

    for (const InfoEntry& info : session.infos)
    {
        if (info.idx >= (int) end)
            break;
        if (StartsWith(info.info, "Exposure = "))
            if (std::optional<double> ms = Positive(info.info.substr(11)))
                m.exposure = *ms / 1000.;
        if (info.idx < (int) begin)
            continue;
        if (StartsWith(info.info, "DITHER"))
            m.dithers++;
        else if (StartsWith(info.info, "Settling failed"))
            m.unsettled++;
    }

    // the best and worst 5 minutes, over full windows only
    const double window = 300.;
    if (end > begin && entries[end - 1].dt - entries[begin].dt >= 2 * window)
    {
        GuideSession::EntryVec stretch(entries.begin() + begin, entries.begin() + end);
        std::vector<double> rms = RunningRms(stretch, window);
        std::optional<size_t> worst;
        double best = INFINITY;
        for (size_t i = 0; i < stretch.size(); i++)
        {
            if (stretch[i].dt - stretch[0].dt < window || isnan(rms[i]))
                continue;
            best = std::min(best, rms[i]);
            if (!worst || rms[i] > rms[*worst])
                worst = i;
        }
        if (worst)
        {
            m.bestWindow = best * session.pixelScale;
            m.worstWindow = rms[*worst] * session.pixelScale;
            size_t first = *worst;
            while (first > 0 && stretch[first - 1].dt > stretch[*worst].dt - window)
                first--;
            m.worst = IndexRun{ (unsigned int) (begin + first), (unsigned int) (begin + *worst) };

            for (const InfoEntry& info : session.infos)
            {
                if (info.idx < (int) m.worst->first || info.idx > (int) m.worst->last + 1)
                    continue;
                if (StartsWith(info.info, "DITHER"))
                    m.worstDithers++;
                if (!StartsWith(info.info, "CONDITIONS"))
                    continue;
                for (const char *key : { "Wind gust", "Wind speed" })
                    if (std::optional<double> wind = Leading(Setting(info.info, key)))
                        m.worstWind = std::max(m.worstWind.value_or(0.), *wind);
            }
        }
    }
    return m;
}

namespace {

std::string ClockAt(const GuideSession& session, double dt)
{
    if (!session.starts.IsValid())
        return std::string();
    return Line((session.starts + wxTimeSpan::Milliseconds((wxLongLong) (dt * 1000.))).Format("%H:%M"));
}

std::string Degrees(double v)
{
    return Fixed(v, 1) + "\u00b0";
}

Finding Note(const std::string& title, const std::string& body, const std::string& evidence = std::string())
{
    Finding f;
    f.title = title;
    f.body = body;
    f.evidence = evidence;
    return f;
}

} // namespace

Assessment Assess(const GuideLog& log, const GuideSession& session, size_t begin, size_t end,
                  const GuideStats& stats)
{
    Assessment a;
    const double scale = session.pixelScale;
    const double total = hypot(stats.rms_ra, stats.rms_dec) * scale;
    const Measurements m = Measure(session, begin, end);
    const ImagingTrain train = ImagingTrainFrom(session.hdr);
    a.total = Arcsec(total);

    // the verdict, against the imaging pixel
    if (train.pixelScale)
    {
        a.ratio = total / *train.pixelScale;
        std::string what;
        if (a.ratio <= 0.5)
        {
            a.verdict = Verdict::Excellent;
            what = " Under half a pixel: as good as guiding gets.";
        }
        else if (a.ratio <= 1.)
        {
            a.verdict = Verdict::Good;
            what = " Under one pixel, stars stay round.";
        }
        else if (a.ratio <= 1.5)
        {
            a.verdict = Verdict::Fair;
            what = " Over one pixel, stars may look slightly stretched.";
        }
        else
        {
            a.verdict = Verdict::Poor;
            what = " Stars will look stretched or bloated.";
        }
        static const char *const labels[] = { "Excellent", "Good", "Fair", "Poor" };
        a.verdictLabel = labels[(int) a.verdict];
        // the camera and focal length, not the telescope: Alpaca's telescope
        // is the mount, so its name is usually the mount's
        std::string with = train.camera.empty() ? std::string() : ", " + train.camera;
        if (train.focalLength)
            with += (train.camera.empty() ? ", " : " at ") + Fixed(*train.focalLength, 0) + " mm";
        a.scale = Fixed(a.ratio, 2) + " of an imaging pixel (" + Fixed(*train.pixelScale, 2) + ARCSEC + "/px" + with +
                  ")." + what;
    }
    else
        a.scale = "This log doesn't say which telescope and camera took the images, so guiding can't be compared "
                  "with your image's pixels. Logs from OpenAstro PHD2 and OpenAstro Guider say.";

    // why the total is what it is
    a.why = a.total + " is " + Fixed(total / scale, 2) + " of a guide-camera pixel (" + Fixed(scale, 2) + ARCSEC +
            "/px).";
    if (m.hfd > 0. && m.medianSnr > 0.)
        a.why += " At SNR " + Fixed(m.medianSnr, 0) + " the guide camera can place the star only to within about " +
                 Arcsec(0.85 * m.hfd / m.medianSnr * scale) + ", so part of the total is measurement, not movement.";
    bool random = m.frames >= 30 && m.raLag1 >= -0.3 && m.raLag1 <= 0.3;
    if (random)
        a.why += " What's left changes randomly from frame to frame. Mount errors build up over minutes, so random "
                 "motion like this is mostly the sky (seeing), which no setting can remove.";
    if (std::optional<SkyFloor> floor = SkyFloorFor(log, session))
    {
        a.why += " The Guiding Assistant measured the sky alone at " + Arcsec(floor->total);
        if (!floor->sameSession)
            a.why += " earlier in the night";
        double extra = (total / floor->total - 1.) * 100.;
        if (extra <= 5.)
            a.why += ": guiding is as good as the sky allowed.";
        else if (extra <= 50.)
            a.why += ", so guiding is within " + Fixed(extra, 0) + "% of what the sky allowed.";
        else
            a.why += ", so guiding added " + Fixed(extra, 0) + "% on top of the sky; the suggestions below may help.";
    }
    else
        a.why += " This log has no Guiding Assistant run. Run it for 2 minutes at the start of a night to measure "
                 "how much the sky alone moves the star.";

    auto improve = [&a](Finding f) { a.improve.push_back(std::move(f)); };
    auto good = [&a](const std::string& title, const std::string& body) { a.good.push_back(Note(title, body)); };
    const std::string corr = "Frame-to-frame correlation " + Fixed(m.raLag1, 2);
    const std::string raPulses = "RA pulses on " + Percent(m.raPulsed) + " of frames";

    // RA: does it overshoot, lag, or chase noise?
    if (m.frames >= 30 && m.raLag1 < -0.3)
    {
        Finding f = Note("RA overcorrects",
                   "Each RA correction overshoots, so the next frame's error points the other way.", corr + " \u00b7 " +
                   raPulses);
        if (m.raAggression > 0.)
            f.changes.push_back(Change{ "RA aggression", Percent(m.raAggression),
                                        Percent(std::max(0.2, ToTwentieth(m.raAggression * 0.7))) });
        improve(f);
    }
    else if (m.frames >= 30 && m.raLag1 > 0.5)
    {
        Finding f = Note("RA corrections fall behind",
                   "RA errors carry over from frame to frame: corrections are too gentle to keep up, or the mount "
                   "has periodic error or drift bigger than they can follow. The Frequency chart shows which.",
                   corr);
        if (m.raAggression > 0. && m.raAggression < 1.)
            f.changes.push_back(Change{ "RA aggression", Percent(m.raAggression),
                                        Percent(std::min(1., ToTwentieth(m.raAggression * 1.3))) });
        improve(f);
    }

    if (m.frames > 0 && m.lost > 0.02 * (m.frames + m.lost))
        improve(Note("Star lost on " + std::to_string(m.lost) + " frames",
                         "The guide star wasn't found on " + std::to_string(m.lost) + " of " +
                         std::to_string(m.frames + m.lost) + " frames. Thin cloud, dew or a faint star can cause it; a "
                         "brighter star or a longer exposure helps."));
    else if (m.frames > 0)
        good("Star held", (m.lost ? "Lost on only " + std::to_string(m.lost) + " frames" : std::string("No lost frames")) +
                          (m.medianSnr > 0. ? "; SNR " + Fixed(m.medianSnr, 0) + "." : "."));
    if (m.medianSnr > 0. && m.medianSnr < 10.)
        improve(Note("Faint guide star",
                         "At SNR " + Fixed(m.medianSnr, 0) + " the star's position is hard to measure, so guiding "
                         "reacts to measurement noise. Pick a brighter star or lengthen the exposure."));

    const double maxDur = session.mount.xlim.maxDur;
    if (maxDur > 0. && m.frames > 0)
    {
        std::string limit = Seconds(maxDur / 1000.);
        if (m.maxedPulses > 0.01 * m.frames)
            improve(Note("Pulses hit the " + limit + " limit",
                             std::to_string(m.maxedPulses) + " pulses were as long as allowed, so PHD2 couldn't fully "
                             "correct them. That usually means a sudden jump (wind, a cable snag) or a guide rate that's "
                             "too slow."));
        else
            good("Pulses within limits", m.maxedPulses ? "Only " + std::to_string(m.maxedPulses) + " reached the " +
                                                         limit + " maximum." :
                                                         "None reached the " + limit + " maximum.");
    }

    bool square = m.orthoError < 0. || m.orthoError <= 10.;
    bool near = isnan(m.dec) || isnan(m.calDec) || fabs(m.dec - m.calDec) <= 30.;
    if (!square)
        improve(Note("Calibration is not square",
                         "RA and Dec moved " + Degrees(m.orthoError) + " away from a right angle while calibrating, "
                         "usually a sign of backlash or too short a calibration. Recalibrate."));
    if (!near)
        improve(Note("Calibrated far from here",
                         "Calibrated at Dec " + Degrees(m.calDec) + ", guiding at Dec " + Degrees(m.dec) +
                         ". Recalibrate near where you're imaging."));
    if (square && near && (m.orthoError >= 0. || !isnan(m.calDec)))
    {
        std::string body;
        if (!isnan(m.calDec) && !isnan(m.dec))
            body = "Calibrated at Dec " + Degrees(m.calDec) + ", guiding at " + Degrees(m.dec);
        if (m.orthoError >= 0.)
            body += (body.empty() ? "" : "; ") + Degrees(m.orthoError) + " from square";
        good("Calibration is current", body + ".");
    }

    double span = end > begin && end <= session.entries.size() ?
        session.entries[end - 1].dt - session.entries[begin].dt : 0.;
    if (span >= 300. && stats.paerr > 0.)
    {
        std::string drift = Fixed(stats.drift_dec * scale, 2) + ARCSEC + "/min";
        if (stats.paerr > 10.)
            improve(Note("Polar alignment is " + Fixed(stats.paerr, 0) + ARCMIN + " off",
                             "Dec drifts " + drift + ". Guiding corrects it, but better polar alignment leaves Dec "
                             "less to do and avoids field rotation."));
        else
            good("Polar alignment " + Fixed(stats.paerr, 1) + ARCMIN, "Dec drift is " + drift + ".");
    }

    // RA errors that don't carry over, pulsed often with short exposures
    if (m.frames >= 30 && m.raLag1 >= -0.3 && m.raLag1 <= 0.5 && m.exposure > 0. && m.exposure < 2. &&
        m.raPulsed > 0.4)
    {
        std::string body = "RA errors don't carry over from one frame to the next, so most RA pulses answer random "
                           "motion that's gone by the next frame.";
        if (m.raAggression > 0.)
        {
            double cost = stats.rms_ra * scale * (1. - 1. / sqrt(1. + m.raPulsed * m.raAggression * m.raAggression));
            body += " That adds about " + Arcsec(cost) + " to RA.";
        }
        body += " Longer exposures average the seeing out, leaving less to chase.";
        Finding f = Note("RA is correcting noise", body, corr + " \u00b7 " + raPulses);
        f.changes.push_back(Change{ "Exposure", Fixed(m.exposure, 1) + " s", "2\u20133 s" });
        if (m.raMinMove > 0. && m.raMinMove < 0.2)
            f.changes.push_back(Change{ "RA minimum move", Fixed(m.raMinMove, 2) + " px",
                                        Fixed(std::max(0.2, UpToTwentieth(m.raMinMove * 1.5)), 2) + " px" });
        improve(f);
    }

    // Dec pushed back and forth with little drift to correct
    size_t decPulses = m.north + m.south;
    if (m.frames >= 30 && m.decPulsed > 0.2 && std::min(m.north, m.south) >= 0.3 * decPulses &&
        fabs(stats.drift_dec * scale) < 0.5)
    {
        Finding f = Note("Dec is pushed both ways",
                   "Dec hardly drifts (" + Fixed(stats.drift_dec * scale, 2) + ARCSEC + "/min), yet PHD2 pulsed north " +
                   std::to_string(m.north) + " times and south " + std::to_string(m.south) +
                   " times. Pulses that alternate like this mostly follow noise.",
                   "Dec pulses on " + Percent(m.decPulsed) + " of frames");
        if (m.decMinMove > 0.)
            f.changes.push_back(Change{ "Dec minimum move", Fixed(m.decMinMove, 2) + " px",
                                        Fixed(std::max(0.2, UpToTwentieth(m.decMinMove * 1.5)), 2) + " px" });
        improve(f);
    }

    if (m.dithers > 0)
    {
        if (m.unsettled > 0)
            improve(Note(std::to_string(m.unsettled) + " of " + std::to_string(m.dithers) +
                             " dithers did not settle",
                             "Give settling more time or a wider tolerance in your imaging app, or look at what "
                             "happened after those dithers on the timeline."));
        else
            good("Dithers settled", std::to_string(m.dithers) + (m.dithers == 1 ? " dither, settled." :
                                                                   " dithers, all settled."));
    }

    if (m.worst && m.worstWindow >= 1.4 * m.bestWindow)
    {
        const auto& e = session.entries;
        double middle = (e[m.worst->first].dt + e[m.worst->last].dt) / 2.;
        Finding f = Note("Worse around " + ClockAt(session, middle),
                   "Over any 5 minutes, RMS ranged from " + Arcsec(m.bestWindow) + " to " + Arcsec(m.worstWindow) + ".");
        if (m.worstWind)
            f.body += " The wind reached " + Fixed(*m.worstWind, 1) + " m/s then, the likely cause.";
        else
            f.body += " A change like that usually comes from the sky or a gust, not from settings.";
        if (m.worstDithers)
            f.body += std::string(" The stretch includes a ") + (m.worstDithers == 1 ? "dither" : "few dithers") +
                      "; frames just after one, before the star is back on target, add to it.";
        f.frames = m.worst;
        improve(f);
    }

    return a;
}
