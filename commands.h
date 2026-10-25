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

#ifndef COMMANDS_INCLUDED
#define COMMANDS_INCLUDED

// Menu command IDs shared by the main menus and the charts' context menus.
// The main window handles them all; context menus send them up to it.

#include <wx/defs.h>

enum
{
    ID_OPEN_IN_EDITOR = wxID_HIGHEST + 1,
    ID_VIEW_TIMELINE,
    ID_VIEW_SCATTER,
    ID_VIEW_DRIFT,
    ID_VIEW_FREQUENCY,
    ID_TOGGLE_SIDEBAR,
    ID_TOGGLE_INSPECTOR,
    ID_INCLUDE_ALL,
    ID_EXCLUDE_ALL,
    ID_REAPPLY_RULES,
    ID_DESELECT,
    ID_EXCLUDE_SELECTION,
    ID_INCLUDE_SELECTION,
    ID_ZOOM_IN,
    ID_ZOOM_OUT,
    ID_ZOOM_FIT,
    ID_ZOOM_SELECTION,
    ID_FIT_VERTICAL,
    ID_LOCK_VERTICAL,
    ID_EXPORT_CSV,
    ID_SELECT_GA,
    ID_PREVIOUS_SECTION,
    ID_NEXT_SECTION,
    ID_CLOSE_LOG,
};

#endif
