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

#ifndef SESSIONDETAILS_INCLUDED
#define SESSIONDETAILS_INCLUDED

// A section's header as labelled fields for the inspector, independent of
// the GUI so it can be tested.

#include <wx/arrstr.h>

#include <string>
#include <vector>

struct DetailField
{
    std::string label;
    std::string value; // UTF-8
};

// The equipment and settings a section's header describes, in a fixed
// order: Profile, Camera, Mount, AO, Pixel scale, Exposure, the guide
// algorithms and Pointing. Fields the header does not mention are left out.
std::vector<DetailField> SessionDetails(const wxArrayString& hdr);

#endif
