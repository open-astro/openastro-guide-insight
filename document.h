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

#ifndef DOCUMENT_INCLUDED
#define DOCUMENT_INCLUDED

// The open log and what the user is looking at in it, independent of the
// GUI so it can be tested: the current section, the frames selected on the
// chart, each session's view, and the exclusion settings.

#include "analysis.h"
#include "exclusion.h"
#include "graphmath.h"
#include "logparser.h"
#include "selection.h"
#include "viewport.h"

#include <iosfwd>
#include <optional>
#include <vector>

// The rules that exclude frames automatically.
struct ExclusionSettings
{
    bool settlingByServer = true;
    bool settlingByDistance = false;
    SettleParams settle{ 1.0, 10.0 };
    bool serverPauses = false;
};

// Includes every frame of the session, then excludes the frames the
// settings pick out.
void ApplyExclusions(GuideSession& session, const ExclusionSettings& settings);

// The largest RA or Dec offset in the session, in guide-camera pixels.
double MaxOffset(const GuideSession& session);

// What one session's chart shows.
struct SessionView
{
    Viewport horizontal{ 1. };
    VerticalView vertical;
};

class Document
{
    GuideLog m_log;
    ExclusionSettings m_exclusions;
    std::optional<size_t> m_current; // index into m_log.sections
    std::optional<IndexRun> m_selection;
    XAxis m_axis = XAxis::Frames;
    double m_lock = 0.; // locked vertical scale, screen pixels per arc-second
    std::vector<SessionView> m_views; // one per session

    void ResetViews();

public:
    // Reads a log, applies the exclusion settings to every session and works
    // out the statistics. The first section becomes current. If the stream
    // cannot be read, returns false and keeps the log already open.
    bool Load(std::istream& is);

    const GuideLog& Log() const { return m_log; }

    // Changes the exclusion rules and applies them again to every session,
    // which also undoes frames included or excluded by hand.
    void SetExclusions(const ExclusionSettings& settings);
    const ExclusionSettings& Exclusions() const { return m_exclusions; }

    // The current section, an index into Log().sections, if there is one.
    std::optional<size_t> Current() const { return m_current; }
    // Makes a section current and clears the selection.
    void Select(size_t section);

    // The current section if it is a guiding session or a calibration.
    GuideSession *Session();
    const GuideSession *Session() const;
    const Calibration *CalibrationSection() const;

    // Frames selected in the current session.
    void SetSelection(const std::optional<IndexRun>& run);
    const std::optional<IndexRun>& Selection() const { return m_selection; }
    // Statistics of the selection, or of the whole session without one.
    GuideStats Stats() const;
    // Includes or excludes the selection, or every frame without one.
    void IncludeSelection(bool include);

    // The current session's view. While the vertical scale is locked, its
    // vertical scale is the locked one.
    SessionView *View();

    // What the x-axis counts. Changing it keeps each session showing the same frames.
    void SetXAxis(XAxis axis);
    XAxis GetXAxis() const { return m_axis; }

    // Fits the current session's vertical view to a chart this tall.
    void FitVertical(double height);
    void ZoomVertical(double factor);

    // A locked vertical scale is shared by every session, in screen pixels
    // per arc-second, so sessions with different pixel scales compare
    // directly. Unlocking leaves every session at the locked scale.
    void LockVerticalScale(bool lock);
    bool VerticalScaleLocked() const { return m_lock > 0.; }
    // The locked scale, or 0 when unlocked, for saving between runs.
    double VerticalLock() const { return m_lock; }
    void SetVerticalLock(double pixelsPerArcsec) { m_lock = pixelsPerArcsec > 0. ? pixelsPerArcsec : 0.; }
};

#endif
