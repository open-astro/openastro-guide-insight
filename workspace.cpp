/*
 *  This file is part of OpenAstro Guide Insight, a fork of PHD2 Log Viewer
 *
 *  Copyright (C) 2026 Open Astro contributors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 */

#include "workspace.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

namespace {

std::filesystem::path Identity(const wxString& path)
{
    std::error_code error;
    std::filesystem::path p(path.fn_str().data());
    std::filesystem::path resolved = std::filesystem::weakly_canonical(p, error);
    if (error)
    {
        error.clear();
        resolved = std::filesystem::absolute(p, error).lexically_normal();
        if (error)
            resolved = p.lexically_normal();
    }
    return resolved;
}

} // namespace

bool Workspace::Open(const wxString& path, const ExclusionSettings& exclusions, XAxis axis,
                     double verticalLock, const std::function<void()>& progress)
{
    std::string identity = Identity(path);
    for (const auto& entry : m_entries)
        if (entry->identity == identity)
        {
            m_active = entry.get();
            return true;
        }

    std::ifstream ifs(path.fn_str());
    if (!ifs)
        return false;

    auto entry = std::make_unique<Entry>();
    entry->path = path;
    entry->identity = std::move(identity);
    entry->document.SetExclusions(exclusions);
    entry->document.SetXAxis(axis);
    entry->document.SetVerticalLock(verticalLock);
    if (!entry->document.Load(ifs, progress))
        return false;
    for (size_t i = 0; i < entry->document.Log().sections.size(); i++)
        if (entry->document.Log().sections[i].type == GUIDING_SECTION)
        {
            entry->document.Select(i);
            break;
        }

    m_active = entry.get();
    m_entries.push_back(std::move(entry));
    return true;
}

bool Workspace::Activate(size_t index)
{
    if (index >= m_entries.size())
        return false;
    m_active = m_entries[index].get();
    return true;
}

bool Workspace::Close(size_t index)
{
    if (index >= m_entries.size())
        return false;
    Entry *closing = m_entries[index].get();
    bool active = closing == m_active;
    m_entries.erase(m_entries.begin() + index);
    if (active)
        m_active = m_entries.empty() ? nullptr : m_entries[std::min(index, m_entries.size() - 1)].get();
    return true;
}

size_t Workspace::ActiveIndex() const
{
    for (size_t i = 0; i < m_entries.size(); i++)
        if (m_entries[i].get() == m_active)
            return i;
    return m_entries.size();
}

const wxString& Workspace::ActivePath() const
{
    static const wxString empty;
    return m_active ? m_active->path : empty;
}
