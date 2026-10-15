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

#include <catch2/catch_session.hpp>
#include <wx/init.h>

int main(int argc, char *argv[])
{
    // wxString, wxDateTime and wxRegEx need the wxWidgets base library
    // initialised, but no GUI.
    wxInitializer init;
    if (!init.IsOk())
        return 1;

    return Catch::Session().run(argc, argv);
}
