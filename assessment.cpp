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

// A Guiding Assistant result's "<key>=  0.21 px (...)" value, in pixels.
double HpfPx(const std::string& info, const std::string& key)
{
    size_t pos = info.find(key + "=");
    return pos == std::string::npos ? 0. : Positive(info.substr(pos + key.size() + 1)).value_or(0.);
}

std::optional<SkyFloor> LastFloor(const GuideSession& s, bool same)
{
    std::optional<SkyFloor> floor;
    for (const InfoEntry& info : s.infos)
        if (std::optional<double> v = TotalHpfRms(info.info, s.pixelScale))
            floor = SkyFloor{ *v, same, HpfPx(info.info, "RA HPF-RMS"), HpfPx(info.info, "Dec HPF-RMS") };
    return floor;
}

} // namespace

MountKind KindOfMount(const std::string& name)
{
    wxString n = wxString::FromUTF8(name).Upper();
    static const char *const strainWave[] = { "HAE", "HEM", "AM3", "AM5", "RST-", "NYX-", "WAVE 100", "WAVE 150",
                                              "HARMONIC", "STRAIN WAVE", "ZWO AM" };
    static const char *const geared[] = { "EQ6", "EQ8", "EQ5", "EQ3", "HEQ5", "AZ-EQ", "EQM-35", "CEM", "GEM", "CGX",
                                          "CGEM", "AVX", "LOSMANDY", "G11", "PARAMOUNT", "MACH1", "MACH2", "IEXOS",
                                          "EXOS", "10MICRON", "EQ-R" };
    for (const char *k : strainWave)
        if (n.Contains(k))
            return MountKind::StrainWave;
    for (const char *k : geared)
        if (n.Contains(k))
            return MountKind::Geared;
    return MountKind::Unknown;
}

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
    if (std::optional<SkyFloor> own = LastFloor(session, true))
        return own;

    // the latest result in an earlier session
    for (size_t i = 0; i < log.sessions.size(); i++)
    {
        if (&log.sessions[i] != &session)
            continue;
        for (size_t j = i; j-- > 0;)
            if (std::optional<SkyFloor> earlier = LastFloor(log.sessions[j], false))
                return earlier;
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
    std::vector<double> mass, snrs; // in frame order
    size_t raPulses = 0, decPulses = 0;
    int lastDec = 0; // the direction of the last Dec pulse
    for (size_t i = begin; i < end; i++)
    {
        const GuideEntry& e = entries[i];
        if (e.included && e.guiding && !StarWasFound(e.err))
            m.lost++;
        if (!Counted(e))
            continue;
        m.frames++;
        snr.push_back(e.snr);
        mass.push_back(e.mass);
        snrs.push_back(e.snr);
        if (e.err == 1)
            m.saturated++;
        if (i + 1 < end && Counted(entries[i + 1]))
        {
            ra.emplace_back(e.raraw, entries[i + 1].raraw);
            dec.emplace_back(e.decraw, entries[i + 1].decraw);
        }
        if (e.mount != MOUNT)
            continue;
        if (e.radur)
        {
            raPulses++;
            (e.radur > 0 ? m.east : m.west)++;
        }
        if (e.decdur)
        {
            decPulses++;
            (e.decdur > 0 ? m.north : m.south)++;
            int dir = e.decdur > 0 ? 1 : -1;
            if (lastDec && dir != lastDec)
            {
                m.decReversals++;
                auto side = [](double v) { return (v > 0.) - (v < 0.); };
                if (i + 2 < end && Counted(entries[i + 1]) && Counted(entries[i + 2]) &&
                    side(entries[i + 1].decraw) == side(e.decraw) && side(entries[i + 2].decraw) == side(e.decraw))
                    m.decStuck++;
            }
            lastDec = dir;
        }
        const Mount& mount = session.mount;
        if ((mount.xlim.maxDur > 0. && abs(e.radur) >= mount.xlim.maxDur) ||
            (mount.ylim.maxDur > 0. && abs(e.decdur) >= mount.ylim.maxDur))
            m.maxedPulses++;
    }
    auto median = [](std::vector<double> v) {
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return v[v.size() / 2];
    };
    if (mass.size() >= 40)
    {
        size_t q = mass.size() / 4;
        m.firstMass = median(std::vector<double>(mass.begin(), mass.begin() + q));
        m.lastMass = median(std::vector<double>(mass.end() - q, mass.end()));
        m.firstSnr = median(std::vector<double>(snrs.begin(), snrs.begin() + q));
        m.lastSnr = median(std::vector<double>(snrs.end() - q, snrs.end()));
    }
    if (end > begin + 10)
    {
        std::vector<double> gaps;
        for (size_t i = begin + 1; i < end; i++)
            gaps.push_back(entries[i].dt - entries[i - 1].dt);
        m.cadence = median(gaps);
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
        else if (StartsWith(line, "Mount = "))
            m.mount = line;
        else if (StartsWith(line, "Max RA duration = "))
            m.decGuideMode = Setting(line, "DEC guide mode");
        else if (StartsWith(line, "RA Guide Speed = "))
        {
            m.guideSpeedRa = Positive(Setting(line, "RA Guide Speed")).value_or(0.);
            m.guideSpeedDec = Positive(Setting(line, "Dec Guide Speed")).value_or(0.);
            m.calDec = Leading(Setting(line, "Cal Dec")).value_or(NAN);
        }
        else if (StartsWith(line, "Norm rates RA = "))
        {
            m.calRateRa = Positive(line.substr(16)).value_or(0.);
            size_t dec = line.find(", Dec = ");
            if (dec != std::string::npos)
                m.calRateDec = Positive(line.substr(dec + 8)).value_or(0.);
            size_t ortho = line.find("ortho.err. = ");
            if (ortho != std::string::npos)
                m.orthoError = Leading(line.substr(ortho + 13)).value_or(-1.);
        }
        else if (StartsWith(line, "Pixel scale = "))
            m.binning = (int) Positive(Setting(line, "Binning")).value_or(0.);
        else if (StartsWith(line, "Dither = "))
            m.timeLapse = Leading(Setting(line, "Guide-frame time lapse")).value_or(0.) / 1000.;
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

    std::vector<double> ditherSizes, settleTimes;
    std::optional<double> settleStart;
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
        {
            m.dithers++;
            // "DITHER by 4.116, -3.024"
            std::optional<double> dx = Leading(info.info.substr(std::min<size_t>(10, info.info.size())));
            size_t comma = info.info.find(", ");
            std::optional<double> dy = comma == std::string::npos ? std::nullopt : Leading(info.info.substr(comma + 2));
            if (dx && dy)
                ditherSizes.push_back(hypot(*dx, *dy));
        }
        else if (StartsWith(info.info, "Settling failed"))
            m.unsettled++;
        double at = entries.empty() ? 0. : entries[std::min<size_t>(info.idx, entries.size() - 1)].dt;
        if (StartsWith(info.info, "Settling started"))
            settleStart = at;
        else if (StartsWith(info.info, "Settling complete") && settleStart)
        {
            settleTimes.push_back(at - *settleStart);
            settleStart.reset();
        }
    }

    if (!ditherSizes.empty())
        m.ditherSize = median(ditherSizes);
    if (!settleTimes.empty())
        m.settleTime = median(settleTimes);

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
    const bool backlash = m.decReversals >= 10 && m.decStuck >= 0.5 * m.decReversals;
    const double overhead = m.cadence - m.exposure - m.timeLapse;
    const bool slowLoop = m.exposure > 0. && m.cadence > 0. && overhead > 1. && overhead > m.exposure;
    const MountKind kind = KindOfMount(m.mount);
    const std::optional<SkyFloor> sky = SkyFloorFor(log, session);
    // PHD2's Guiding Assistant minimum moves, from the measured Dec noise
    double raRec = 0., decRec = 0.;
    if (sky && sky->decPx > 0.)
    {
        decRec = std::max(0.1, sky->decPx * (scale < 1.5 ? 1.28 : 1.65));
        raRec = std::max(0.1, decRec * 0.65);
    }
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

    if (slowLoop)
        improve(Note("Frames arrive every " + Fixed(m.cadence, 1) + " s",
                     "Each exposure is " + Fixed(m.exposure, 1) + " s, but a new frame arrives only every " +
                     Fixed(m.cadence, 1) + " s: about " + Fixed(overhead, 1) + " s of each cycle goes into getting "
                     "the image to the guider. Corrections act on where the star was a few seconds before. Guide on a "
                     "smaller area of the camera (a subframe), bin the guide camera, or use a faster connection to it.",
                     "Median time between frames " + Fixed(m.cadence, 2) + " s"));

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

    if (scale > 0. && scale < 0.5 && m.binning == 1)
    {
        Finding f = Note("Bin the guide camera",
                         "Each guide pixel covers only " + Fixed(scale, 2) + ARCSEC + ", smaller than guiding needs. "
                         "Binning makes the star brighter and faster to read, with no loss of accuracy (PHD2's "
                         "Guiding Assistant suggests the same).");
        f.changes.push_back(Change{ "Guide camera binning", "1", std::to_string((int) ceil(0.5 / scale - 1e-9)) });
        improve(f);
    }

    if (m.frames > 0 && m.saturated > 0.05 * m.frames)
        improve(Note("Guide star saturated",
                     "The star was saturated on " + std::to_string(m.saturated) + " of " + std::to_string(m.frames) +
                     " frames, so its centre can't be measured precisely. Pick a fainter star or shorten the exposure."));
    if (m.firstMass > 0. && m.lastMass < 0.6 * m.firstMass)
    {
        std::string snrs = m.firstSnr > 0. ? " (SNR " + Fixed(m.firstSnr, 0) + " \u2192 " + Fixed(m.lastSnr, 0) + ")" :
                                              std::string();
        improve(Note("Guide star faded",
                     "By the end, the star was " + Percent(m.lastMass / m.firstMass) + " as bright as at the start" +
                     snrs + ". Dew on the guide scope, haze, or the star sinking lower in the sky can do this. Check "
                     "for dew, or pick a star higher in the sky."));
    }

    const double sidereal = 15.04;
    if (m.guideSpeedRa > 0. && m.guideSpeedRa < 0.45 * sidereal)
    {
        Finding f = Note("Guide speed is slow",
                         "The mount guides at " + Fixed(m.guideSpeedRa / sidereal, 2) + "\u00d7 sidereal, so every "
                         "correction takes a long pulse and big errors take long to recover. Most mounts guide best "
                         "at 0.5\u20130.9\u00d7. Recalibrate after changing it.");
        f.changes.push_back(Change{ "Guide speed", Fixed(m.guideSpeedRa / sidereal, 2) + "\u00d7 sidereal",
                                    "0.5\u20130.9\u00d7 sidereal" });
        improve(f);
    }

    auto offRate = [](double measured, double speed) {
        return measured > 0. && speed > 0. && (measured < 0.5 * speed || measured > 1.5 * speed);
    };
    if (offRate(m.calRateRa, m.guideSpeedRa) || offRate(m.calRateDec, m.guideSpeedDec))
        improve(Note("Calibration doesn't match the guide speed",
                     "Calibration measured the mount moving " + Fixed(m.calRateRa, 1) + ARCSEC + "/s in RA and " +
                     Fixed(m.calRateDec, 1) + ARCSEC + "/s in Dec, but it reports a guide speed of " +
                     Fixed(m.guideSpeedRa, 1) + ARCSEC + "/s. Check the guide speed set in the mount or its driver, "
                     "and that the clutches aren't slipping; then recalibrate."));

    // the calibration this session used: the last one before it in the log
    const Calibration *cal = nullptr;
    for (const Calibration& c : log.calibrations)
        if (c.starts.IsValid() && session.starts.IsValid() && c.starts <= session.starts)
            cal = &c;
    if (cal)
    {
        int west = 0, north = 0;
        for (const CalibrationEntry& e : cal->entries)
        {
            if (e.direction == WEST)
                west++;
            else if (e.direction == NORTH)
                north++;
        }
        if ((west > 0 && west < 8) || (north > 0 && north < 8))
            improve(Note("Calibration was short",
                         "Calibration took " + std::to_string(west) + " steps west and " + std::to_string(north) +
                         " north; PHD2 aims for about 12. With fewer steps the rates are measured less precisely. "
                         "Use PHD2's calibration step calculator to pick a smaller step, then recalibrate."));
    }

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

    size_t raPulseCount = m.east + m.west;
    if (raPulseCount >= 50 && std::max(m.east, m.west) >= 0.8 * raPulseCount)
    {
        bool east = m.east > m.west;
        std::string body = "PHD2 pushed RA " + std::string(east ? "east" : "west") + " on " +
                           Percent((double) std::max(m.east, m.west) / raPulseCount) + " of its RA pulses, so "
                           "something keeps pulling the star the other way: polar alignment, a tracking rate that's "
                           "slightly off";
        if (kind == MountKind::StrainWave)
            body += ". Strain-wave mounts carry no counterweight, so weight isn't the cause.";
        else
            body += ", or the mount's balance. A slightly east-heavy balance is fine; a big imbalance strains the "
                    "gears.";
        body += " Steady one-way pulses are guiding doing its job; they're only a problem when they get long.";
        improve(Note("RA is pushed mostly one way", body,
                     std::to_string(m.east) + " east, " + std::to_string(m.west) + " west"));
    }

    double span = end > begin && end <= session.entries.size() ?
        session.entries[end - 1].dt - session.entries[begin].dt : 0.;
    if (span >= 300. && stats.paerr > 0.)
    {
        std::string drift = Fixed(stats.drift_dec * scale, 2) + ARCSEC + "/min";
        // PHD2's Guiding Assistant: over 5' could be improved, over 10' use
        // drift alignment; with likely Dec backlash, only over 10'
        if (stats.paerr > (backlash ? 10. : 5.))
            improve(Note("Polar alignment is " + Fixed(stats.paerr, 0) + ARCMIN + " off",
                         "Dec drifts " + drift + ". Guiding corrects it, but better polar alignment leaves Dec less "
                         "to do and avoids field rotation. " +
                         (stats.paerr > 10. ? std::string("Use the drift-align tool, or your polar alignment routine, "
                                                          "to improve it.") :
                                              std::string("It could be improved."))));
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
        if (slowLoop)
            body += " With frames this slow, a longer exposure barely slows guiding.";
        if (kind == MountKind::StrainWave)
            body += " Strain-wave mounts like this one have fast periodic error: if the Frequency chart then shows "
                    "peaks under 30 s, go back to a shorter exposure.";
        Finding f = Note("RA is correcting noise", body, corr + " \u00b7 " + raPulses);
        f.changes.push_back(Change{ "Exposure", Fixed(m.exposure, 1) + " s", "2\u20134 s" });
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
                                        Fixed(decRec > 0. ? decRec :
                                                  std::max(0.2, UpToTwentieth(m.decMinMove * 1.5)), 2) + " px" });
        improve(f);
    }

    auto far = [](double current, double rec) { return current > 0. && rec > 0. && fabs(current - rec) > 0.4 * rec; };
    if (far(m.raMinMove, raRec) || far(m.decMinMove, decRec))
    {
        Finding f = Note("Minimum move doesn't match the measured noise",
                         "The Guiding Assistant measured how much the star jumps around on its own: " +
                         Fixed(sky->raPx, 2) + " px in RA and " + Fixed(sky->decPx, 2) + " px in Dec. PHD2 sets the "
                         "minimum move from that, so guiding ignores the jumps but still catches real drift.");
        f.changes.push_back(Change{ "RA minimum move", Fixed(m.raMinMove, 2) + " px", Fixed(raRec, 2) + " px" });
        f.changes.push_back(Change{ "Dec minimum move", Fixed(m.decMinMove, 2) + " px", Fixed(decRec, 2) + " px" });
        improve(f);
    }

    size_t decPulseCount = m.north + m.south;
    if (decPulseCount >= 20 && std::max(m.north, m.south) >= 0.9 * decPulseCount && m.decGuideMode == "Auto")
    {
        bool north = m.north > m.south;
        Finding f = Note("Dec only needs pushing one way",
                         "All but " + std::to_string(std::min(m.north, m.south)) + " of " +
                         std::to_string(decPulseCount) + " Dec pulses went " + (north ? "north" : "south") +
                         ": Dec drifts steadily one way. Guiding Dec in that direction only stops noise pushing it "
                         "back and forth, and sidesteps Dec backlash.",
                         std::to_string(m.north) + " north, " + std::to_string(m.south) + " south");
        f.changes.push_back(Change{ "Dec guide mode", "Auto", north ? "North" : "South" });
        improve(f);
    }

    if (backlash)
        improve(Note("Dec may have backlash",
                     "After Dec changed direction, the error stayed on the same side for two more frames " +
                     std::to_string(m.decStuck) + " times out of " + std::to_string(m.decReversals) +
                     "; random motion would do that about 1 time in 4. That's what slack in the Dec gears looks "
                     "like. The Guiding Assistant's backlash test measures it. If Dec drifts steadily one way, "
                     "guide Dec in that direction only; otherwise try backlash compensation.",
                     std::to_string(m.decStuck) + " of " + std::to_string(m.decReversals) + " Dec reversals"));

    if (m.ditherSize > 0. && train.pixelScale)
    {
        double pixels = m.ditherSize * scale / *train.pixelScale;
        std::string size = "Dithers moved about " + Fixed(pixels, 0) + " imaging pixels";
        if (pixels < 3.)
            improve(Note("Dithers are small",
                         size + ", too little to break up the patterns noise leaves when frames are stacked. Raise "
                         "the dither amount in your imaging app to move at least 3\u20135 pixels."));
        else if (pixels > 25.)
            improve(Note("Dithers are bigger than needed",
                         size + ". 5\u201310 pixels is enough; bigger dithers take longer to settle and cost imaging "
                         "time."));
        else
            good("Dither size", size + ".");
    }
    if (m.settleTime && *m.settleTime > 30.)
        improve(Note("Dithers take long to settle",
                     "Settling took " + Fixed(*m.settleTime, 0) + " s after each dither (the median), time lost from "
                     "imaging. A smaller dither, a wider settle tolerance in your imaging app, or a higher aggression "
                     "helps."));

    if (m.dithers > 0)
    {
        if (m.unsettled > 0)
            improve(Note(std::to_string(m.unsettled) + " of " + std::to_string(m.dithers) +
                             " dithers did not settle",
                             "Give settling more time or a wider tolerance in your imaging app, or look at what "
                             "happened after those dithers on the timeline."));
        else
            good("Dithers settled", std::to_string(m.dithers) + (m.dithers == 1 ? " dither, settled" :
                                                                   " dithers, all settled") +
                                    (m.settleTime ? " in about " + Fixed(*m.settleTime, 0) + " s" : std::string()) + ".");
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
