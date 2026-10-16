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
    double mx = 0.;
    for (const auto& e : session.entries)
        mx = std::max({ mx, (double) fabs(e.raraw), (double) fabs(e.decraw) });
    return mx;
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
}

void Document::Select(size_t section)
{
    if (section >= m_log.sections.size())
        return;
    m_current = section;
    m_selection.reset();
}

const GuideSession *Document::Session() const
{
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
    if (!m_current)
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

void Document::IncludeSelection(bool include)
{
    GuideSession *s = Session();
    if (!s)
        return;
    if (m_selection)
        ::IncludeFrames(*s, *m_selection, include);
    else
        IncludeRange(s->entries, include);
    s->CalcStats();
}

void Document::IncludeFrames(const IndexRun& run, bool include)
{
    GuideSession *s = Session();
    if (!s || run.first > run.last)
        return;
    ::IncludeFrames(*s, run, include);
    s->CalcStats();
}

void Document::IncludeAll(bool include)
{
    GuideSession *s = Session();
    if (!s)
        return;
    IncludeRange(s->entries, include);
    s->CalcStats();
}

SessionView *Document::View()
{
    GuideSession *s = Session();
    if (!s)
        return nullptr;
    SessionView *v = &m_views[m_log.sections[*m_current].idx];
    if (VerticalScaleLocked())
        v->vertical.SetScale(m_lock * s->pixelScale);
    return v;
}

void Document::SetXAxis(XAxis axis)
{
    if (axis == m_axis)
        return;

    for (size_t i = 0; i < m_views.size(); i++)
    {
        const auto& entries = m_log.sessions[i].entries;
        Viewport& h = m_views[i].horizontal;
        double first = IndexAtPosition(entries, h.First(), m_axis);
        double last = IndexAtPosition(entries, h.Last(), m_axis);
        double width = h.Width();

        h = NewViewport(entries, axis);
        h.SetWidth(width);
        h.Show(PositionAtIndex(entries, first, axis), PositionAtIndex(entries, last, axis));
    }
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
        for (size_t i = 0; i < m_views.size(); i++)
            m_views[i].vertical.SetScale(m_lock * m_log.sessions[i].pixelScale);
    m_lock = 0.;
}
