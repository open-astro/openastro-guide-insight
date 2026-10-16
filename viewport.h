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

#ifndef VIEWPORT_INCLUDED
#define VIEWPORT_INCLUDED

// Which part of a session a chart shows, and where it lands on screen,
// independent of the GUI so it can be tested.

#include "logparser.h"

// The horizontal view. Positions along the x-axis are frame indices, or
// seconds from the start of the session for a time axis (XAxis below).
// The view never shows beyond the ends of the content, so zooming out stops
// when the whole session is visible.
class Viewport
{
    double m_maxScale;
    double m_start = 0.;
    double m_end = 0.;
    double m_width = 0.;
    double m_scale = 1.;  // pixels per unit of position
    double m_offset = 0.; // position at the left edge

    double FitScale() const;
    void Clamp();

public:
    // maxScale is the closest zoom, in pixels per unit of position.
    explicit Viewport(double maxScale);

    // The positions the content spans. The view fits the whole content.
    void SetContent(double start, double end);

    // The view's width in pixels. The visible range stays the same.
    void SetWidth(double width);

    // Shows the whole content, or as much as fits at the closest zoom.
    void Fit();

    // Zooms in (factor > 1) or out, keeping the position under x in place.
    void ZoomAbout(double factor, double x);

    // Moves the view dx pixels later (or earlier when dx < 0).
    void Scroll(double dx);

    // Shows the range from first to last, as nearly as the limits allow.
    void Show(double first, double last);

    double XAt(double position) const;
    double PositionAt(double x) const;

    double First() const { return m_offset; }
    double Last() const { return m_offset + m_width / m_scale; }
    double Scale() const { return m_scale; }
    double Width() const { return m_width; }
    bool CanZoomIn() const;
    bool CanZoomOut() const;
};

// The vertical view, in guide-camera pixels: zero sits at mid-height, moved
// by the pan offset.
class VerticalView
{
    double m_scale = 1.;  // screen pixels per guide-camera pixel
    double m_offset = 0.; // screen pixels the zero line is moved down

public:
    // Scales so that maxValue reaches 10 pixels from the top and bottom, and
    // puts zero back at mid-height.
    void Fit(double maxValue, double height);

    void Zoom(double factor) { m_scale *= factor; }
    void SetScale(double scale) { m_scale = scale; }
    // Moves the zero line dy pixels down.
    void Pan(double dy) { m_offset += dy; }

    double YAt(double value, double height) const;
    double ValueAt(double y, double height) const;

    double Scale() const { return m_scale; }
};

// What the timeline's x-axis counts.
enum class XAxis { Frames, Time };

// Where entry i sits on the x-axis.
double AxisPosition(const GuideSession::EntryVec& entries, size_t i, XAxis axis);

// Where a fractional entry index sits on the x-axis. On a time axis it
// interpolates between frames, and clamps to the first and last.
double PositionAtIndex(const GuideSession::EntryVec& entries, double index, XAxis axis);

// The entry at an x-axis position, as a fractional index.
double IndexAtPosition(const GuideSession::EntryVec& entries, double position, XAxis axis);

// The span of the x-axis from the first entry to the last.
void AxisSpan(const GuideSession::EntryVec& entries, XAxis axis, double *start, double *end);

#endif
