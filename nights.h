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

#ifndef NIGHTS_INCLUDED
#define NIGHTS_INCLUDED

// Tonight's guiding against earlier nights: each log summarised, the
// settings that changed between two nights, and whether guiding got better,
// independent of the GUI so it can be tested.

#include "logparser.h"

#include <wx/datetime.h>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct NightSummary
{
    std::string path;
    wxDateTime start;           // when the first guiding session began
    std::string profile;        // the equipment profile
    size_t frames = 0;          // frames counted over all guiding sessions
    double total = 0., ra = 0., dec = 0.; // RMS over all guiding sessions, arc-seconds
    std::optional<double> sky;  // the Guiding Assistant's floor, arc-seconds
    // the guiding settings, as (name, value) in a fixed order, from the
    // longest guiding session
    std::vector<std::pair<std::string, std::string>> settings;
};

// A log's guiding, with the default exclusions applied; nothing when it has
// no guiding session with enough frames to judge.
std::optional<NightSummary> SummarizeNight(const GuideLog& log, const std::string& path);

struct SettingChange
{
    std::string setting;
    std::string from;
    std::string to;
};
std::vector<SettingChange> SettingChanges(const NightSummary& before, const NightSummary& after);

struct NightComparison
{
    NightSummary earlier;
    std::string text;                      // "Since 2 October: total RMS 2.30″ → 2.08″ (−10%)."
    std::vector<SettingChange> changes;
    std::string trend;                     // "Last 4 nights: 2.40″ · 2.31″ · 2.20″ · 2.08″", or empty
};

// Tonight against the latest earlier night with the same equipment profile.
std::optional<NightComparison> CompareNights(const NightSummary& tonight, const std::vector<NightSummary>& nights);

// The other PHD2 guide logs in the same folder as `path`, newest first, at
// most `max` of them.
std::vector<std::string> GuideLogsBeside(const std::string& path, size_t max = 30);

// Summaries of logs, read once and read again only when a file changes.
class NightCache
{
    struct Entry
    {
        long long size;
        time_t modified;
        std::optional<NightSummary> summary;
    };
    std::map<std::string, Entry> m_entries;

public:
    // How many logs have been read, for tests.
    size_t reads = 0;
    std::optional<NightSummary> Summary(const std::string& path);
    std::vector<NightSummary> Summaries(const std::vector<std::string>& paths);
};

#endif
