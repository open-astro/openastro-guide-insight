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

#include "viewport.h"

#include "graphmath.h"

#include <algorithm>

// Room for rounding error when comparing scales.
static const double EPS = 1e-9;

Viewport::Viewport(double maxScale) : m_maxScale(maxScale) { }

// The scale that shows the whole content, which is also the farthest zoom.
double Viewport::FitScale() const
{
    double len = m_end - m_start;
    if (m_width <= 0. || len <= 0.)
        return m_maxScale;
    return std::min(m_width / len, m_maxScale);
}

void Viewport::Clamp()
{
    m_scale = std::clamp(m_scale, FitScale(), m_maxScale);

    double span = m_width / m_scale;
    if (span >= m_end - m_start)
        m_offset = m_start;
    else
        m_offset = std::clamp(m_offset, m_start, m_end - span);
}

void Viewport::SetContent(double start, double end)
{
    m_start = start;
    m_end = std::max(start, end);
    Fit();
}

void Viewport::SetWidth(double width)
{
    if (m_width > 0. && width > 0.)
    {
        double first = First(), last = Last();
        m_width = width;
        Show(first, last);
    }
    else
    {
        m_width = std::max(width, 0.);
        Fit();
    }
}

void Viewport::Fit()
{
    m_scale = FitScale();
    m_offset = m_start;
}

void Viewport::ZoomAbout(double factor, double x)
{
    double position = PositionAt(x);
    m_scale = std::clamp(m_scale * factor, FitScale(), m_maxScale);
    m_offset = position - x / m_scale;
    Clamp();
}

void Viewport::Scroll(double dx)
{
    m_offset += dx / m_scale;
    Clamp();
}

void Viewport::Show(double first, double last)
{
    if (first > last)
        std::swap(first, last);
    if (last > first && m_width > 0.)
        m_scale = m_width / (last - first);
    m_offset = first;
    Clamp();
}

double Viewport::XAt(double position) const
{
    return (position - m_offset) * m_scale;
}

double Viewport::PositionAt(double x) const
{
    return m_offset + x / m_scale;
}

bool Viewport::CanZoomIn() const
{
    return m_scale < m_maxScale * (1. - EPS);
}

bool Viewport::CanZoomOut() const
{
    return m_scale > FitScale() * (1. + EPS);
}

void VerticalView::Fit(double maxValue, double height)
{
    m_scale = maxValue > 0. ? (height / 2. - 10.) / maxValue : 1.;
    m_offset = 0.;
}

double VerticalView::YAt(double value, double height) const
{
    return height / 2. + m_offset - value * m_scale;
}

double VerticalView::ValueAt(double y, double height) const
{
    return (height / 2. + m_offset - y) / m_scale;
}

double AxisPosition(const GuideSession::EntryVec& entries, size_t i, XAxis axis)
{
    return axis == XAxis::Time ? (double) entries[i].dt : (double) i;
}

double IndexAtPosition(const GuideSession::EntryVec& entries, double position, XAxis axis)
{
    return axis == XAxis::Time ? IndexAtTime(entries, position) : position;
}

void AxisSpan(const GuideSession::EntryVec& entries, XAxis axis, double *start, double *end)
{
    if (entries.empty())
    {
        *start = *end = 0.;
        return;
    }
    *start = AxisPosition(entries, 0, axis);
    *end = AxisPosition(entries, entries.size() - 1, axis);
}
