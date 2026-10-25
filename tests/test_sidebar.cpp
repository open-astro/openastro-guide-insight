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

#include "SessionSidebar.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Selecting a log header toggles its collapsed state", "[sidebar]")
{
    std::vector<wxString> collapsed;
    wxString first = "first.log";
    wxString second = "second.log";

    CHECK_FALSE(ToggleCollapsedLog(collapsed, first));
    CHECK(ToggleCollapsedLog(collapsed, first));
    CHECK_FALSE(ToggleCollapsedLog(collapsed, second));
    CHECK(collapsed == std::vector<wxString>{ second });
}

TEST_CASE("A session selection cannot combine rows from another log", "[sidebar]")
{
    std::vector<SidebarRow> rows = {
        { 0, std::nullopt, {} },
        { 0, 2, {} },
        { 0, 3, {} },
        { 1, std::nullopt, {} },
        { 1, 1, {} },
    };

    CHECK(SelectedSections(rows, 2, { 2, 4 }) == std::vector<size_t>{ 3 });
    CHECK(SelectedSections(rows, 2, {}) == std::vector<size_t>{ 3 });
}
