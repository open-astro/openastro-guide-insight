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

#include "version.h"

#include <catch2/catch_test_macros.hpp>
#include <wx/version.h>

#include <sstream>
#include <string>

// MSVC only reports the real standard in __cplusplus with /Zc:__cplusplus;
// _MSVC_LANG is always accurate there.
#if defined(_MSVC_LANG)
#define GUIDEINSIGHT_CPLUSPLUS _MSVC_LANG
#else
#define GUIDEINSIGHT_CPLUSPLUS __cplusplus
#endif

TEST_CASE("Compiled as C++20 or newer", "[build]")
{
    CHECK(GUIDEINSIGHT_CPLUSPLUS >= 202002L);
}

#ifdef GUIDEINSIGHT_EXPECTED_WX_VERSION
// Release builds pin an exact wxWidgets version; the preset or CI job passes
// it in. Local development builds against system packages skip this check.
TEST_CASE("Built against the pinned wxWidgets version", "[build]")
{
    std::ostringstream actual;
    actual << wxMAJOR_VERSION << '.' << wxMINOR_VERSION << '.' << wxRELEASE_NUMBER;
    CHECK(actual.str() == GUIDEINSIGHT_EXPECTED_WX_VERSION);
}
#endif

// The version is written once, in CMakeLists.txt's project() call. The
// tests are built with it passed in separately, so a stale or hand-edited
// version.h shows up here.
TEST_CASE("The app's version is the project's version", "[build]")
{
    CHECK(std::string(GUIDEINSIGHT_VERSION) == GUIDEINSIGHT_PROJECT_VERSION);
}
