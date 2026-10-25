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

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <filesystem>
#include <chrono>

TEST_CASE("Opening another log keeps the first document alive", "[workspace]")
{
    Workspace workspace;
    wxString first = wxString::FromUTF8(TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt");

    REQUIRE(workspace.Open(first));
    Document *original = &workspace.Active();
    REQUIRE(original);
    original->Select(2);

    REQUIRE(workspace.Open(first));
    CHECK(&workspace.Active() == original);
    CHECK(original->Current() == 2);
    CHECK(workspace.Size() == 1);

    REQUIRE(workspace.Open(wxString::FromUTF8(TEST_DATA_DIR "/./openastro_redcat51_2026-10-04.txt")));
    CHECK(workspace.Size() == 1);
    CHECK(&workspace.Active() == original);
    CHECK(original->Current() == 2);
}

TEST_CASE("Separate log paths have separate stable documents", "[workspace]")
{
    Workspace workspace;
    wxString first = wxString::FromUTF8(TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt");
    auto copy = std::filesystem::temp_directory_path() /
        ("guideinsight-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    std::filesystem::copy_file(first.fn_str().data(), copy);

    REQUIRE(workspace.Open(first));
    Document *original = &workspace.Active();
    original->Select(2);
    original->SetSelection(IndexRun{ 20, 30 });
    original->SetVerticalLock(1.25);
    REQUIRE(workspace.Open(wxString::FromUTF8(copy.string())));
    workspace.Active().SetVerticalLock(2.5);
    CHECK(workspace.Size() == 2);
    CHECK(&workspace.Active() != original);
    CHECK(workspace.Activate(0));
    CHECK(&workspace.Active() == original);
    CHECK(workspace.At(0).Current() == 2);
    REQUIRE(workspace.At(0).Selection());
    CHECK(workspace.At(0).Selection()->first == 20);
    CHECK(workspace.Active().VerticalLock() == 1.25);
    REQUIRE(workspace.Activate(1));
    CHECK(workspace.Active().VerticalLock() == 2.5);

    std::filesystem::remove(copy);
}

TEST_CASE("A failed open leaves the active log alone", "[workspace]")
{
    Workspace workspace;
    wxString first = wxString::FromUTF8(TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt");
    REQUIRE(workspace.Open(first));
    Document *original = &workspace.Active();

    CHECK_FALSE(workspace.Open("/no/such/guide-log.txt"));
    CHECK(workspace.Size() == 1);
    CHECK(&workspace.Active() == original);
}

TEST_CASE("A log path with Unicode characters can be opened and deduplicated", "[workspace]")
{
    Workspace workspace;
    wxString first = wxString::FromUTF8(TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt");
    std::string name = "guideinsight-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-\xC3\xA9.txt";
    auto copy = std::filesystem::temp_directory_path() /
        std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(name.data()), name.size()));
    std::filesystem::copy_file(first.fn_str().data(), copy);
    auto utf8 = copy.u8string();
    wxString path = wxString::FromUTF8(reinterpret_cast<const char *>(utf8.c_str()));

    REQUIRE(workspace.Open(path));
    REQUIRE(workspace.Open(path));
    CHECK(workspace.Size() == 1);

    std::filesystem::remove(copy);
}

TEST_CASE("Closing the active log chooses the next or previous entry", "[workspace]")
{
    Workspace workspace;
    wxString first = wxString::FromUTF8(TEST_DATA_DIR "/openastro_redcat51_2026-10-04.txt");
    auto copy = std::filesystem::temp_directory_path() /
        ("guideinsight-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    std::filesystem::copy_file(first.fn_str().data(), copy);

    REQUIRE(workspace.Open(first));
    Document *original = &workspace.Active();
    REQUIRE(workspace.Open(wxString::FromUTF8(copy.string())));
    REQUIRE(workspace.Activate(0));
    REQUIRE(workspace.Close(1));
    CHECK(&workspace.Active() == original);
    REQUIRE(workspace.Open(wxString::FromUTF8(copy.string())));
    REQUIRE(workspace.Close(1));
    CHECK(&workspace.Active() == original);
    REQUIRE(workspace.Close(0));
    CHECK(workspace.Size() == 0);

    std::filesystem::remove(copy);
}
