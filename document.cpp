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

#include "document.h"

#include <algorithm>
#include <istream>
#include <string>
#include <utility>
#include <math.h>

// The closest zoom on a frame axis, in pixels per frame.
static const double MAX_PIXELS_PER_FRAME = 100.;

void ApplyExclusions(GuideSession& session, const ExclusionSettings& settings)
{
    IncludeRange(session.entries, true);
    if (settings.settlingByServer)
        ExcludeSettlingByServer(session);
    if (settings.settlingByDistance)
        ExcludeSettlingByDistance(session, settings.settle);
    if (settings.serverPauses)
        ExcludeServerPauses(session);
}

double MaxOffset(const GuideSession& session)
{
    // the frames that count, unless none do
    double counted = 0., all = 0.;
    bool any = false;
    for (const auto& e : session.entries)
    {
        double v = std::max(fabs(e.raraw), fabs(e.decraw));
        all = std::max(all, v);
        if (e.included && StarWasFound(e.err))
        {
            counted = std::max(counted, v);
            any = true;
        }
    }
    return any ? counted : all;
}

// The closest zoom: the same for a time axis as for a frame axis, taking
// the session's average time between frames.
static double MaxScale(const GuideSession::EntryVec& entries, XAxis axis)
{
    double start, end;
    AxisSpan(entries, axis, &start, &end);
    if (axis == XAxis::Time && end > start)
        return MAX_PIXELS_PER_FRAME * (double) (entries.size() - 1) / (end - start);
    return MAX_PIXELS_PER_FRAME;
}

static Viewport NewViewport(const GuideSession::EntryVec& entries, XAxis axis)
{
    Viewport v(MaxScale(entries, axis));
    double start, end;
    AxisSpan(entries, axis, &start, &end);
    v.SetContent(start, end);
    return v;
}

void Document::ResetViews()
{
    m_views.clear();
    for (const auto& s : m_log.sessions)
        m_views.push_back(SessionView{ NewViewport(s.entries, m_axis), VerticalView() });
}

bool Document::Load(std::istream& is, const std::function<void()>& progress)
{
    if (!is)
        return false;

    GuideLog log;
    LogParser parser;
    parser.progress = progress;
    parser.Parse(is, log);
    m_log = std::move(log);

    for (auto& s : m_log.sessions)
    {
        ApplyExclusions(s, m_exclusions);
        s.CalcStats();
    }

    ResetViews();
    m_combinedSections.clear();
    m_selection.reset();
    m_current.reset();
    if (!m_log.sections.empty())
        m_current = 0;
    return true;
}

void Document::SetExclusions(const ExclusionSettings& settings)
{
    m_exclusions = settings;
    for (auto& s : m_log.sessions)
    {
        ApplyExclusions(s, m_exclusions);
        s.CalcStats();
    }
    if (Combined())
        BuildCombined();
}

void Document::Select(size_t section)
{
    if (section >= m_log.sections.size())
        return;
    m_current = section;
    m_combinedSections.clear();
    m_selection.reset();
}

void Document::SelectSections(const std::vector<size_t>& sections)
{
    std::vector<size_t> guiding;
    for (size_t i = 0; i < m_log.sections.size(); i++)
        if (m_log.sections[i].type == GUIDING_SECTION &&
            std::find(sections.begin(), sections.end(), i) != sections.end())
            guiding.push_back(i);

    if (guiding.size() < 2)
    {
        if (!guiding.empty())
            Select(guiding.front());
        else if (!sections.empty())
            Select(sections.front());
        return;
    }

    m_current = guiding.front();
    m_combinedSections = guiding;
    m_selection.reset();
    BuildCombined();
    m_combinedView = SessionView{ NewViewport(m_combined.entries, m_axis), VerticalView() };
}

std::vector<size_t> Document::Sections() const
{
    if (Combined())
        return m_combinedSections;
    if (m_current)
        return { *m_current };
    return {};
}

void Document::BuildCombined()
{
    const GuideSession& first = m_log.sessions[m_log.sections[m_combinedSections.front()].idx];
    m_combined = GuideSession(first.date);
    m_combined.starts = first.starts;
    m_combined.hdr = first.hdr;
    m_combined.pixelScale = first.pixelScale;
    m_combined.declination = first.declination;
    m_combined.mount = first.mount;
    m_combined.ao = first.ao;
    m_sources.clear();

    double end = 0.; // the last frame's time so far, for sessions without a start time
    for (size_t section : m_combinedSections)
    {
        size_t si = m_log.sections[section].idx;
        const GuideSession& s = m_log.sessions[si];
        double offset = s.starts.IsValid() && first.starts.IsValid() ?
            (s.starts - first.starts).GetMilliseconds().ToDouble() / 1000. : end;
        // distances in the first session's pixels
        float k = (float) (first.pixelScale > 0. ? s.pixelScale / first.pixelScale : 1.);
        int base = (int) m_combined.entries.size();

        InfoEntry marker;
        marker.idx = base;
        marker.repeats = 1;
        marker.info = "Session " + std::string(s.starts.IsValid() ? s.starts.Format("%H:%M:%S").utf8_str()
                                                                     : s.date.utf8_str());
        m_combined.infos.push_back(marker);
        for (InfoEntry info : s.infos)
        {
            info.idx += base;
            m_combined.infos.push_back(info);
        }

        for (size_t i = 0; i < s.entries.size(); i++)
        {
            GuideEntry e = s.entries[i];
            e.dt = (float) (e.dt + offset);
            for (float *v : { &e.dx, &e.dy, &e.raraw, &e.decraw, &e.raguide, &e.decguide })
                *v *= k;
            m_combined.entries.push_back(e);
            m_sources.emplace_back(si, i);
        }
        if (!m_combined.entries.empty())
            end = m_combined.entries.back().dt;
    }
    m_combined.duration = end;
    m_combined.CalcStats();
}

const GuideSession *Document::Session() const
{
    if (Combined())
        return &m_combined;
    if (!m_current)
        return nullptr;
    const LogSectionLoc& loc = m_log.sections[*m_current];
    return loc.type == GUIDING_SECTION ? &m_log.sessions[loc.idx] : nullptr;
}

GuideSession *Document::Session()
{
    return const_cast<GuideSession *>(static_cast<const Document *>(this)->Session());
}

const Calibration *Document::CalibrationSection() const
{
    if (!m_current || Combined())
        return nullptr;
    const LogSectionLoc& loc = m_log.sections[*m_current];
    return loc.type == CALIBRATION_SECTION ? &m_log.calibrations[loc.idx] : nullptr;
}

void Document::SetSelection(const std::optional<IndexRun>& run)
{
    const GuideSession *s = Session();
    if (s && run && run->first <= run->last && run->last < s->entries.size())
        m_selection = run;
    else
        m_selection.reset();
}

GuideStats Document::Stats() const
{
    const GuideSession *s = Session();
    if (!s)
        return GuideStats();
    if (m_selection)
        return CalcStats(*s, m_selection->first, m_selection->last + 1);
    return CalcStats(*s, 0, s->entries.size());
}

void Document::SetIncluded(const IndexRun& run, bool include)
{
    if (!Combined())
    {
        GuideSession *s = Session();
        if (!s || run.first > run.last)
            return;
        ::IncludeFrames(*s, run, include);
        s->CalcStats();
        return;
    }

    std::vector<bool> touched(m_log.sessions.size(), false);
    for (size_t i = run.first; i <= run.last && i < m_sources.size(); i++)
    {
        auto [session, entry] = m_sources[i];
        m_log.sessions[session].entries[entry].included = include;
        touched[session] = true;
    }
    for (size_t i = 0; i < touched.size(); i++)
        if (touched[i])
            m_log.sessions[i].CalcStats();
    BuildCombined();
}

void Document::IncludeSelection(bool include)
{
    GuideSession *s = Session();
    if (!s || s->entries.empty())
        return;
    SetIncluded(m_selection ? *m_selection : IndexRun{ 0, (unsigned int) s->entries.size() - 1 }, include);
}

void Document::IncludeFrames(const IndexRun& run, bool include)
{
    if (Session())
        SetIncluded(run, include);
}

void Document::IncludeAll(bool include)
{
    GuideSession *s = Session();
    if (!s || s->entries.empty())
        return;
    SetIncluded(IndexRun{ 0, (unsigned int) s->entries.size() - 1 }, include);
}

SessionView *Document::View()
{
    GuideSession *s = Session();
    if (!s)
        return nullptr;
    SessionView *v = Combined() ? &m_combinedView : &m_views[m_log.sections[*m_current].idx];
    if (VerticalScaleLocked())
        v->vertical.SetScale(m_lock * s->pixelScale);
    return v;
}

void Document::SetXAxis(XAxis axis)
{
    if (axis == m_axis)
        return;

    auto convert = [this, axis](const GuideSession::EntryVec& entries, Viewport& h) {
        double first = IndexAtPosition(entries, h.First(), m_axis);
        double last = IndexAtPosition(entries, h.Last(), m_axis);
        double width = h.Width();

        h = NewViewport(entries, axis);
        h.SetWidth(width);
        h.Show(PositionAtIndex(entries, first, axis), PositionAtIndex(entries, last, axis));
    };
    for (size_t i = 0; i < m_views.size(); i++)
        convert(m_log.sessions[i].entries, m_views[i].horizontal);
    if (Combined())
        convert(m_combined.entries, m_combinedView.horizontal);
    m_axis = axis;
}

void Document::FitVertical(double height)
{
    SessionView *v = View();
    if (!v)
        return;
    v->vertical.Fit(MaxOffset(*Session()), height);
    v->verticalFitted = true;
    if (VerticalScaleLocked())
        m_lock = v->vertical.Scale() / Session()->pixelScale;
}

void Document::ZoomVertical(double factor)
{
    SessionView *v = View();
    if (!v)
        return;
    if (VerticalScaleLocked())
        m_lock *= factor;
    else
        v->vertical.Zoom(factor);
}

void Document::LockVerticalScale(bool lock)
{
    if (lock)
    {
        if (SessionView *v = View())
            m_lock = v->vertical.Scale() / Session()->pixelScale;
        return;
    }

    if (VerticalScaleLocked())
    {
        for (size_t i = 0; i < m_views.size(); i++)
            m_views[i].vertical.SetScale(m_lock * m_log.sessions[i].pixelScale);
        m_combinedView.vertical.SetScale(m_lock * m_combined.pixelScale);
    }
    m_lock = 0.;
}
