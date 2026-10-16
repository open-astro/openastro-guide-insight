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

#ifndef SETTINGSMIGRATION_INCLUDED
#define SETTINGSMIGRATION_INCLUDED

// Carrying saved settings over when the app's name changes.

#include <wx/config.h>

// If `to` holds no settings yet and `from` does, copies every entry and group
// from `from` into `to` and returns true. Otherwise changes nothing.
bool MigrateSettings(wxConfigBase& from, wxConfigBase& to);

#endif
