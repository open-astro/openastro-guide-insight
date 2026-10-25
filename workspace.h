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

#ifndef WORKSPACE_INCLUDED
#define WORKSPACE_INCLUDED

#include "document.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <wx/string.h>

class Workspace
{
    struct Entry
    {
        wxString path;
        std::filesystem::path identity;
        Document document;
    };

    std::vector<std::unique_ptr<Entry>> m_entries;
    Entry *m_active = nullptr;
    Document m_empty;

public:
    bool Open(const wxString& path, const ExclusionSettings& exclusions = {}, XAxis axis = XAxis::Frames,
              double verticalLock = 0., const std::function<void()>& progress = {});
    bool Close(size_t index);
    bool Activate(size_t index);

    size_t Size() const { return m_entries.size(); }
    size_t ActiveIndex() const;
    Document& Active() { return m_active ? m_active->document : m_empty; }
    const Document& Active() const { return m_active ? m_active->document : m_empty; }
    const wxString& Path(size_t index) const { return m_entries.at(index)->path; }
    Document& At(size_t index) { return m_entries.at(index)->document; }
    const Document& At(size_t index) const { return m_entries.at(index)->document; }
    const wxString& ActivePath() const;
};

#endif
