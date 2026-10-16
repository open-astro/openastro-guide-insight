/*
 *  This file is part of phdlogview
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

#ifndef EXCLUSION_INCLUDED
#define EXCLUSION_INCLUDED

// Choosing which guide frames count toward statistics and analysis.

#include "logparser.h"

struct SettleParams
{
    double pixels;
    double seconds;
};

// Sets entries [i, i1) to included or excluded.
void IncludeRange(GuideSession::EntryVec& entries, bool include, unsigned int i = 0,
                  unsigned int i1 = (unsigned int) -1);

// Excludes frames between the server's "Settling started" and
// "Settling complete" (or "Settling failed") events.
void ExcludeSettlingByServer(GuideSession& session);

// Excludes frames from each dither until the star has stayed within
// params.pixels for longer than params.seconds.
void ExcludeSettlingByDistance(GuideSession& session, const SettleParams& params);

#endif
