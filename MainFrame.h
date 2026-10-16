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

#ifndef MAINFRAME_INCLUDED
#define MAINFRAME_INCLUDED

// The app's window: the session sidebar, the chart and the inspector, with
// a toolbar and menus.

#include "ChartHost.h"
#include "appsettings.h"
#include "document.h"
#include "theme.h"

#include <wx/filehistory.h>
#include <wx/frame.h>

#include <memory>

class ChartView;
class Inspector;
class SegmentedControl;
class SessionSidebar;
class wxPreferencesEditor;
class wxSplitterWindow;

class MainFrame : public wxFrame, public ChartHost
{
    Document m_doc;
    AppSettings m_settings;
    Palette m_palette;
    wxString m_path;
    wxFileHistory m_history;
    std::unique_ptr<wxPreferencesEditor> m_preferences;

    // the Drift and Frequency analysis, worked out when first asked for
    std::unique_ptr<GARun> m_analysis;
    unsigned m_analysisGeneration = 0;
    bool m_analysisStale = true;
    struct AnalysisKey
    {
        const GuideSession *session = nullptr;
        IndexRun range{ 0, 0 };
        AnalysisOptions options;
        bool operator==(const AnalysisKey& o) const
        {
            return session == o.session && range.first == o.range.first && range.last == o.range.last &&
                   options.drift == o.options.drift && options.rawRa == o.options.rawRa;
        }
    } m_analysisKey;

    wxSplitterWindow *m_outer;   // sidebar | the rest
    wxSplitterWindow *m_inner;   // chart | inspector
    SessionSidebar *m_sidebar;
    ChartView *m_chart;
    Inspector *m_inspector;
    SegmentedControl *m_views;
    wxWindow *m_helpWindow = nullptr;

    void BuildMenus();
    void BuildToolbar();
    void BuildLayout();
    void UpdateToolIcons();

    // Brings the window up to date with the document.
    void UpdateAll();
    void UpdateTitle();
    void UpdateChart();
    void SetView(ChartKind view);
    void SelectSection(size_t section);
    void SelectSections(const std::vector<size_t>& sections);
    // The heading over the chart: the section, or the combined sessions.
    wxString Heading() const;
    void ShowSidebar(bool show);
    void ShowInspector(bool show);
    void ApplyPalette();
    void SaveLayout();

    void OnOpen(wxCommandEvent& event);
    void OnOpenRecent(wxCommandEvent& event);
    void OnOpenInEditor(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnIncludeAll(wxCommandEvent& event);
    void OnPreferences(wxCommandEvent& event);
    void OnHelp(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnSysColourChanged(wxSysColourChangedEvent& event);
    void OnSelectAll(wxCommandEvent& event);
    void OnIncludeSelection(wxCommandEvent& event);
    void OnZoom(wxCommandEvent& event);
    void SetDisplay(const DisplayOptions& options);
    void LockVertical(bool lock);
    void ChooseDevice();
    void UpdateSubtitle();
    void OnExportCsv(wxCommandEvent& event);
    void OnSelectGuidingAssistant(wxCommandEvent& event);
    wxString AnalysisScope() const;

public:
    // ChartHost
    Document& Doc() override { return m_doc; }
    const AppSettings& Settings() const override { return m_settings; }
    const Palette& Colors() const override { return m_palette; }
    const GARun *Analysis() override;
    unsigned AnalysisGeneration() const override { return m_analysisGeneration; }
    void FramesChanged() override;
    void SelectionChanged() override;
    void ViewChanged() override;
    void ShowStatus(const wxString& text) override;

    MainFrame();
    ~MainFrame();

    // Opens a guide log, keeping the open one if this one cannot be read.
    void Open(const wxString& path);
};

#endif
