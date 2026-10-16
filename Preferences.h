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

#ifndef PREFERENCES_INCLUDED
#define PREFERENCES_INCLUDED

// The Preferences window: series colours and the settling distance.

#include "appsettings.h"

#include <wx/preferences.h>

#include <functional>
#include <memory>

// `settings` must outlive the editor. `changed` is called after the
// settings change.
std::unique_ptr<wxPreferencesEditor> CreatePreferences(AppSettings& settings, std::function<void()> changed);

#endif
