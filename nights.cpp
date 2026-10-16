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

#include "nights.h"

#include "analysis.h"
#include "assessment.h"
#include "document.h"
#include "labels.h"

#include <wx/dir.h>
#include <wx/filename.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <math.h>

namespace {

const char *const ARCSEC = "″";

bool StartsWith(const std::string& s, const std::string& prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}

// The value of "key = value" in the line, up to the next comma.
std::string Setting(const std::string& line, const std::string& key)
{
    size_t pos = line.find(key + " = ");
    if (pos == std::string::npos)
        return std::string();
    std::string v = line.substr(pos + key.size() + 3);
    return v.substr(0, v.find(", "));
}

// The number a piece of text starts with.
std::optional<double> Leading(const std::string& text)
{
    size_t end = 0;
    while (end < text.size() && (isdigit((unsigned char) text[end]) || text[end] == '.' || text[end] == '-'))
        end++;
    double v;
    if (end == 0 || !wxString::FromUTF8(text.substr(0, end)).ToCDouble(&v))
        return std::nullopt;
    return v;
}

std::string Seconds(double s)
{
    return Fixed(s, s == floor(s) ? 0 : 1) + " s";
}

// An aggression or hysteresis, logged as 0.700 or as 70%.
std::string Percentage(const std::string& value)
{
    std::optional<double> v = Leading(value);
    if (!v)
        return std::string();
    return Fixed(*v <= 1. && value.find('%') == std::string::npos ? *v * 100. : *v, 0) + "%";
}

// The guiding settings of a session, as (name, value) in a fixed order.
std::vector<std::pair<std::string, std::string>> SettingsOf(const GuideSession& s)
{
    Measurements m = Measure(s, 0, s.entries.size());
    std::vector<std::pair<std::string, std::string>> v;
    auto add = [&v](const std::string& name, const std::string& value) {
        if (!value.empty())
            v.emplace_back(name, value);
    };
    std::string xLine, yLine;
    for (const wxString& l : s.hdr)
    {
        std::string line(l.utf8_str());
        if (StartsWith(line, "X guide algorithm = "))
            xLine = line;
        else if (StartsWith(line, "Y guide algorithm = "))
            yLine = line;
    }
    if (m.exposure > 0.)
        add("Exposure", Fixed(m.exposure, 1) + " s");
    add("RA algorithm", Setting(xLine, "X guide algorithm"));
    add("RA aggression", Percentage(Setting(xLine, "Aggression")));
    add("RA hysteresis", Percentage(Setting(xLine, "Hysteresis")));
    if (m.raMinMove > 0.)
        add("RA minimum move", Fixed(m.raMinMove, 2) + " px");
    add("Dec algorithm", Setting(yLine, "Y guide algorithm"));
    add("Dec aggression", Percentage(Setting(yLine, "Aggression")));
    if (m.decMinMove > 0.)
        add("Dec minimum move", Fixed(m.decMinMove, 2) + " px");
    add("Dec guide mode", m.decGuideMode);
    if (s.mount.xlim.maxDur > 0.)
        add("Max RA duration", Seconds(s.mount.xlim.maxDur / 1000.));
    if (s.mount.ylim.maxDur > 0.)
        add("Max Dec duration", Seconds(s.mount.ylim.maxDur / 1000.));
    if (m.guideSpeedRa > 0.)
        add("Guide speed", Fixed(m.guideSpeedRa / 15.04, 2) + "× sidereal");
    if (m.binning > 0)
        add("Guide camera binning", std::to_string(m.binning));
    add("Imaging camera", ImagingTrainFrom(s.hdr).camera);
    return v;
}

std::string Profile(const GuideSession& s)
{
    for (const wxString& l : s.hdr)
        if (l.StartsWith("Equipment Profile = "))
            return std::string(l.Mid(20).utf8_str());
    return std::string();
}

std::string Day(const wxDateTime& t)
{
    return std::to_string(t.GetDay()) + " " + std::string(wxDateTime::GetMonthName(t.GetMonth()).utf8_str());
}

} // namespace

std::optional<NightSummary> SummarizeNight(const GuideLog& log, const std::string& path)
{
    NightSummary n;
    n.path = path;
    double sumRa = 0., sumDec = 0.;
    size_t longest = 0;
    for (const GuideSession& original : log.sessions)
    {
        std::string label = SessionLabel(original);
        if (label == "Guiding Assistant" || label == "Unguided")
            continue;
        GuideSession s = original;
        ApplyExclusions(s, ExclusionSettings());
        GuideStats st = CalcStats(s, 0, s.entries.size());
        if (st.frames < 30)
            continue;
        if (n.frames == 0)
            n.start = s.starts;
        n.frames += st.frames;
        sumRa += st.frames * pow(st.rms_ra * s.pixelScale, 2);
        sumDec += st.frames * pow(st.rms_dec * s.pixelScale, 2);
        if (st.frames > longest)
        {
            longest = st.frames;
            n.profile = Profile(s);
            n.settings = SettingsOf(s);
        }
        if (std::optional<SkyFloor> sky = SkyFloorFor(log, original))
            n.sky = sky->total;
    }
    if (n.frames == 0)
        return std::nullopt;
    n.ra = sqrt(sumRa / n.frames);
    n.dec = sqrt(sumDec / n.frames);
    n.total = hypot(n.ra, n.dec);
    return n;
}

std::vector<SettingChange> SettingChanges(const NightSummary& before, const NightSummary& after)
{
    std::vector<SettingChange> changes;
    for (const auto& [name, value] : after.settings)
        for (const auto& [was, old] : before.settings)
            if (was == name && old != value)
                changes.push_back(SettingChange{ name, old, value });
    return changes;
}

std::optional<NightComparison> CompareNights(const NightSummary& tonight, const std::vector<NightSummary>& nights)
{
    auto comparable = [&tonight](const NightSummary& n) {
        return n.path != tonight.path && n.profile == tonight.profile && n.start.IsValid() &&
               tonight.start.IsValid() && n.start < tonight.start;
    };
    const NightSummary *earlier = nullptr;
    for (const NightSummary& n : nights)
        if (comparable(n) && (!earlier || n.start > earlier->start))
            earlier = &n;
    if (!earlier)
        return std::nullopt;

    NightComparison c;
    c.earlier = *earlier;
    c.changes = SettingChanges(*earlier, tonight);
    double change = (tonight.total / earlier->total - 1.) * 100.;
    c.text = "Since " + Day(earlier->start) + ": total RMS " + Fixed(earlier->total, 2) + ARCSEC + " → " +
             Fixed(tonight.total, 2) + ARCSEC + " (" + Signed(change, 0) + "%).";
    if (c.changes.empty())
        c.text += " No settings changed, so the difference comes from the sky or the setup.";
    else
        c.text += c.changes.size() == 1 ? " One setting changed:" : " These settings changed:";
    if (earlier->sky && tonight.sky)
        c.text += " The Guiding Assistant measured the sky at " + Fixed(*earlier->sky, 2) + ARCSEC + " then and " +
                  Fixed(*tonight.sky, 2) + ARCSEC + " now.";

    // the trend: tonight and the nights before it on the same equipment
    std::vector<const NightSummary *> trend{ &tonight };
    for (const NightSummary& n : nights)
        if (comparable(n))
            trend.push_back(&n);
    std::sort(trend.begin(), trend.end(), [](const NightSummary *a, const NightSummary *b) { return a->start < b->start; });
    if (trend.size() > 5)
        trend.erase(trend.begin(), trend.end() - 5);
    if (trend.size() >= 3)
    {
        c.trend = "Last " + std::to_string(trend.size()) + " nights: ";
        for (size_t i = 0; i < trend.size(); i++)
            c.trend += (i ? " · " : "") + Fixed(trend[i]->total, 2) + ARCSEC;
    }
    return c;
}

std::vector<std::string> GuideLogsBeside(const std::string& path, size_t max)
{
    wxFileName self(wxString::FromUTF8(path));
    wxArrayString files;
    if (!wxDir::Exists(self.GetPath()))
        return {};
    wxDir::GetAllFiles(self.GetPath(), &files, "PHD2_GuideLog_*.txt", wxDIR_FILES);
    files.Sort(true); // the names hold the date, so newest first
    std::vector<std::string> logs;
    for (const wxString& f : files)
    {
        if (wxFileName(f).GetFullName() == self.GetFullName())
            continue;
        if (logs.size() == max)
            break;
        logs.emplace_back(f.utf8_str());
    }
    return logs;
}

std::optional<NightSummary> NightCache::Summary(const std::string& path)
{
    wxFileName fn(wxString::FromUTF8(path));
    if (!fn.FileExists())
        return std::nullopt;
    long long size = (long long) fn.GetSize().GetValue();
    time_t modified = fn.GetModificationTime().GetTicks();
    auto it = m_entries.find(path);
    if (it != m_entries.end() && it->second.size == size && it->second.modified == modified)
        return it->second.summary;

    reads++;
    std::ifstream is(fn.GetFullPath().fn_str());
    GuideLog log;
    LogParser parser;
    std::optional<NightSummary> summary;
    if (is && parser.Parse(is, log))
        summary = SummarizeNight(log, path);
    m_entries[path] = Entry{ size, modified, summary };
    return summary;
}

std::vector<NightSummary> NightCache::Summaries(const std::vector<std::string>& paths)
{
    std::vector<NightSummary> v;
    for (const std::string& p : paths)
        if (std::optional<NightSummary> n = Summary(p))
            v.push_back(*n);
    return v;
}
