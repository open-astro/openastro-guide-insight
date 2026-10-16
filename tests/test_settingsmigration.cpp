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

#include "settingsmigration.h"

#include <catch2/catch_test_macros.hpp>
#include <wx/fileconf.h>
#include <wx/sstream.h>

namespace {

wxFileConfig Config(const char *text)
{
    wxStringInputStream is(text);
    return wxFileConfig(is);
}

wxString Get(wxConfigBase& c, const char *key)
{
    return c.Read(key, "<missing>");
}

} // namespace

TEST_CASE("Settings from the old app name are copied to the new one", "[settings]")
{
    // Before the rename the app stored settings as vendor "adgsoftware",
    // app "phdlogview". Users should keep their colours, exclusion rules and
    // window layout.
    wxFileConfig old = Config(
        "FileOpenDir=/Users/me/PHD2\n"
        "geometry=0;1280;800;10;20\n"
        "[settle]\n"
        "excludeByServer=0\n"
        "pixels=1.5\n"
        "[color]\n"
        "ra=#112233\n");
    wxFileConfig fresh = Config("");

    CHECK(MigrateSettings(old, fresh));

    CHECK(Get(fresh, "/FileOpenDir") == "/Users/me/PHD2");
    CHECK(Get(fresh, "/geometry") == "0;1280;800;10;20");
    CHECK(Get(fresh, "/settle/excludeByServer") == "0");
    CHECK(Get(fresh, "/settle/pixels") == "1.5");
    CHECK(Get(fresh, "/color/ra") == "#112233");
}

TEST_CASE("Settings already saved under the new name are left alone", "[settings]")
{
    wxFileConfig old = Config("[color]\nra=#112233\n");
    wxFileConfig current = Config("[color]\nra=#abcdef\n");

    CHECK_FALSE(MigrateSettings(old, current));
    CHECK(Get(current, "/color/ra") == "#abcdef");
}

TEST_CASE("Nothing to migrate when the old app was never used", "[settings]")
{
    wxFileConfig old = Config("");
    wxFileConfig fresh = Config("");

    CHECK_FALSE(MigrateSettings(old, fresh));
    CHECK_FALSE(fresh.HasEntry("/color/ra"));
}

TEST_CASE("Nested groups are copied", "[settings]")
{
    wxFileConfig old = Config("[a]\nx=1\n[a/b]\ny=2\n[a/b/c]\nz=3\n");
    wxFileConfig fresh = Config("");

    CHECK(MigrateSettings(old, fresh));
    CHECK(Get(fresh, "/a/x") == "1");
    CHECK(Get(fresh, "/a/b/y") == "2");
    CHECK(Get(fresh, "/a/b/c/z") == "3");
}
