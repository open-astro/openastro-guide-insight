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

#include "exclusion.h"

void IncludeRange(GuideSession::EntryVec& entries, bool include, unsigned int i, unsigned int i1)
{
    for (auto it = entries.begin() + i; i < i1 && it != entries.end(); ++it, ++i)
        it->included = include;
}

void ExcludeSettlingByServer(GuideSession& session)
{
    bool settling = false;
    int start_idx = 0;
    auto& infos = session.infos;
    auto& entries = session.entries;

    for (auto it = infos.begin(); it != infos.end(); ++it)
    {
        if (settling)
        {
            if (it->info.find("Settling complete") != std::string::npos || it->info.find("Settling fail") != std::string::npos)
            {
                settling = false;
                IncludeRange(entries, false, start_idx, it->idx);
            }
        }
        else
        {
            if (it->info.find("Settling start") != std::string::npos)
            {
                settling = true;
                start_idx = it->idx;
            }
        }
    }
    if (settling)
        IncludeRange(entries, false, start_idx);
}

void ExcludeSettlingByDistance(GuideSession& session, const SettleParams& params)
{
    auto& infos = session.infos;
    auto& entries = session.entries;
    double lim2 = params.pixels * params.pixels;

    for (auto it = infos.begin(); it != infos.end(); ++it)
    {
        if (it->info.find("DITHER") != std::string::npos)
        {
            if (it->idx >= (int)entries.size())
                break;
            int start_idx = it->idx;
            auto eit = entries.begin() + it->idx;
            double start_time = eit->dt;
            bool close = false;
            bool settled = false;
            int end_idx = start_idx;

            for (; eit != entries.end(); ++eit, ++end_idx)
            {
                double dx = eit->dx;
                double dy = eit->dy;
                double d2 = dx * dx + dy * dy;
                double t = eit->dt;
                if (d2 < lim2)
                {
                    if (!close)
                    {
                        close = true;
                        start_time = t;
                    }
                    else
                    {
                        double elapsed = t - start_time;
                        if (elapsed > params.seconds)
                        {
                            settled = true;
                            break;
                        }
                    }
                }
                else
                {
                    close = false;
                }
            }
            if (settled)
                IncludeRange(entries, false, start_idx, end_idx);
        }
    }
}

void ExcludeServerPauses(GuideSession& session)
{
    bool paused = false;
    int start_idx = 0;

    for (const InfoEntry& info : session.infos)
    {
        if (!paused && info.info == "Server received PAUSE")
        {
            paused = true;
            start_idx = info.idx;
        }
        else if (paused && info.info == "Server received RESUME")
        {
            paused = false;
            IncludeRange(session.entries, false, start_idx, info.idx);
        }
    }
    if (paused)
        IncludeRange(session.entries, false, start_idx);
}
