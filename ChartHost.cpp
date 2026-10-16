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

#include "ChartHost.h"

#include <algorithm>

double WheelPixels(const wxMouseEvent& event)
{
    double notches = (double) event.GetWheelRotation() / std::max(event.GetWheelDelta(), 1);
#ifdef __WXOSX__
    // wxOSX reports a trackpad's pixels as tenths of a notch
    return notches * 10.;
#else
    return notches * event.GetLinesPerAction() * 16.;
#endif
}
