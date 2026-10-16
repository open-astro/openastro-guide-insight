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

#include "MainFrame.h"

#include "ChartView.h"
#include "Inspector.h"
#include "Preferences.h"
#include "SegmentedControl.h"
#include "SessionSidebar.h"
#include "TimelineCanvas.h"
#include "commands.h"
#include "periodogram.h"
#include "icons.h"
#include "labels.h"
#include "timeline.h"

#include <wx/aboutdlg.h>
#include <wx/busyinfo.h>
#include <wx/clipbrd.h>
#include <wx/config.h>
#include <wx/display.h>
#include <wx/file.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/html/htmlwin.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/textctrl.h>
#include <wx/toolbar.h>
#include <wx/utils.h>

#include <fstream>

#define APP_NAME "OpenAstro Guide Insight"
#define APP_VERSION_STR "0.6.5"

namespace {

const wxString VIEW_NAMES[] = { "Timeline", "Scatter", "Drift", "Frequency" };

struct FileDropTarget : public wxFileDropTarget
{
    MainFrame *m_frame;
    explicit FileDropTarget(MainFrame *frame) : m_frame(frame) { }
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override
    {
        if (filenames.empty())
            return false;
        m_frame->Open(filenames[0]);
        return true;
    }
};

wxString U(const std::string& utf8)
{
    return wxString::FromUTF8(utf8);
}

// The Help window's text. CMD, OPT and SHIFT become the platform's names
// for those keys.
const char *HELP_HTML =
    "<html><body>"
    "<h2>OpenAstro Guide Insight</h2>"
    "<p>See how well PHD2 guided: open a guide log, pick a session, and read the chart and the inspector.</p>"

    "<h3>Opening a guide log</h3>"
    "<p>Choose <b>File &rsaquo; Open</b> (CMD+O) or <b>Open Recent</b>, drag a <i>PHD2_GuideLog</i> file onto the "
    "window, or open one from the command line. PHD2 keeps its logs in its own folder, Documents/PHD2 by "
    "default. A debug log is not a guide log; the app says so if you open one.</p>"

    "<h3>The window</h3>"
    "<ul>"
    "<li>The <b>sidebar</b> lists the log's calibrations and guiding sessions with their start, length and total RMS. "
    "Use the arrow keys there, or CMD+OPT+Up and Down anywhere, to move between them.</li>"
    "<li>The <b>toolbar</b> switches the chart between Timeline, Scatter, Drift and Frequency (CMD+1 to CMD+4).</li>"
    "<li>The <b>inspector</b> shows the statistics, drift and polar alignment, the session's equipment and "
    "settings (with the raw header), display options, analysis options and exclusion rules.</li>"
    "<li>Hide the sidebar or the inspector from the View menu or the toolbar.</li>"
    "</ul>"

    "<h3>Timeline</h3>"
    "<ul>"
    "<li>Scroll to pan; pinch or CMD-scroll to zoom about the pointer; OPT-scroll to stretch the vertical axis "
    "and OPT-drag to move it. The strip under the chart shows the whole session: drag its highlight or click to jump.</li>"
    "<li>Point at the chart to read a frame in the status line: its time, offsets, guide pulses, SNR and star mass.</li>"
    "<li>Guide pulses are bars from the zero line: up for West and North pulses, down for East and South.</li>"
    "<li><b>Drag</b> to select frames. The inspector then shows the selection's statistics, and Drift, Frequency "
    "and Scatter analyse just those frames. Esc deselects.</li>"
    "<li>CMD-drag excludes frames from the statistics and SHIFT-drag includes them. CMD-click an excluded "
    "stretch to include it again. Right-click for more.</li>"
    "<li>Display options choose arc-seconds or pixels, RA/Dec or the camera's dx/dy, frames or time across, "
    "and which series to show. <b>Lock vertical scale</b> keeps the same scale, in arc-seconds, for every session.</li>"
    "</ul>"

    "<h3>Scatter</h3>"
    "<p>Each counted frame's RA against Dec, with scale rings and an ellipse two standard deviations wide "
    "that shows any elongation.</p>"

    "<h3>Drift and Frequency</h3>"
    "<p>These analyse the selection, or the whole session without one. <b>Edit &rsaquo; Select Guiding Assistant "
    "Run</b> selects a stretch where PHD2's Guiding Assistant had guide output off.</p>"
    "<ul>"
    "<li><b>Drift</b> shows RA and Dec with the straight-line drift taken out; turn that off in the inspector's "
    "Analysis section to see the raw drift.</li>"
    "<li><b>Frequency</b> is RA's periodogram: periodic error shows as peaks. Point at the curve to read a peak. "
    "<i>Add back RA guide pulses</i> estimates the error guiding was correcting. "
    "<b>File &rsaquo; Export Periodogram as CSV</b> saves it.</li>"
    "</ul>"

    "<h3>Statistics</h3>"
    "<p>Statistics count the frames where the star was found that are not excluded. The exclusion rules leave "
    "out settling after a dither (as PHD2 reports it, or until the star stays within the distance set in "
    "Preferences) and time while a client paused guiding. <b>Edit &rsaquo; Copy Statistics</b> copies them as CSV.</p>"

    "<h3>Calibration</h3>"
    "<p>Choose a calibration to see each step from the starting point, and its rates, angles and orthogonality "
    "error in the inspector. Drag to pan, pinch or CMD-scroll to zoom.</p>"
    "</body></html>";

// The help text in the palette's colours, with the platform's key names.
// Keys are spelled out, as Apple's style guide does on macOS ("Command-O"),
// because the HTML window sets the key symbols off the line.
wxString HelpHtml(const Palette& pal)
{
    wxString html = HELP_HTML;
#ifdef __WXOSX__
    html.Replace("CMD+OPT+", "Command-Option-");
    html.Replace("CMD+", "Command-");
    html.Replace("CMD", "Command");
    html.Replace("OPT", "Option");
    html.Replace("SHIFT", "Shift");
#else
    html.Replace("CMD", "Ctrl");
    html.Replace("OPT", "Alt");
    html.Replace("SHIFT", "Shift");
#endif
    html.Replace("<body>", wxString::Format("<body bgcolor='%s' text='%s'>",
                                            pal.chart.GetAsString(wxC2S_HTML_SYNTAX),
                                            pal.text.GetAsString(wxC2S_HTML_SYNTAX)));
    return html;
}

} // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, APP_NAME, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, "MainFrame"),
      m_settings(LoadSettings(*wxConfig::Get())),
      m_palette(CurrentPalette(m_settings.raColor, m_settings.decColor))
{
    m_doc.SetExclusions(m_settings.exclusions);
    m_doc.SetVerticalLock(m_settings.verticalLock);
    m_doc.SetXAxis(m_settings.display.xaxis);

    BuildMenus();
    BuildToolbar();
    BuildLayout();

    SetMinSize(FromDIP(wxSize(820, 520)));
    SetSize(FromDIP(wxSize(1240, 780)));
    Centre();
    if (m_settings.geometry)
    {
        const Geometry& g = *m_settings.geometry;
        wxRect r(g.x, g.y, g.width, g.height);
        // only where some display can show it
        if (wxDisplay::GetFromPoint(r.GetTopLeft() + wxPoint(40, 10)) != wxNOT_FOUND)
            SetSize(r);
        if (g.maximized)
            Maximize();
    }
    EnableFullScreenView(true);

    // splitting after sizing the frame puts the sashes where they were left
    m_inner->SplitVertically(m_chart, m_inspector, -FromDIP(m_settings.inspectorWidth));
    m_outer->SplitVertically(m_sidebar, m_inner, FromDIP(m_settings.sidebarWidth));
    if (!m_settings.inspectorShown)
        m_inner->Unsplit(m_inspector);
    if (!m_settings.sidebarShown)
        m_outer->Unsplit(m_sidebar);

    SetDropTarget(new FileDropTarget(this));

    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    Bind(wxEVT_SYS_COLOUR_CHANGED, &MainFrame::OnSysColourChanged, this);

    ApplyPalette();
    SetView(m_settings.view);
    UpdateAll();
}

MainFrame::~MainFrame() = default;

void MainFrame::BuildMenus()
{
    auto file = new wxMenu;
    file->Append(wxID_OPEN, "&Open...\tCtrl+O");
    auto recent = new wxMenu;
    file->AppendSubMenu(recent, "Open &Recent");
    file->AppendSeparator();
    file->Append(ID_OPEN_IN_EDITOR, "Open in &Text Editor", "Open the log file in your text editor");
    file->Append(ID_EXPORT_CSV, "&Export Periodogram as CSV...", "Save the Frequency chart's periods and amplitudes");
    file->AppendSeparator();
    file->Append(wxID_CLOSE, "&Close Window\tCtrl+W");
#ifndef __WXOSX__
    file->Append(wxID_EXIT, "E&xit");
#endif

    auto edit = new wxMenu;
    edit->Append(wxID_COPY, "&Copy Statistics\tCtrl+C", "Copy the statistics as CSV");
    edit->AppendSeparator();
    edit->Append(wxID_SELECTALL, "Select &All Frames\tCtrl+A");
    edit->Append(ID_DESELECT, "&Deselect\tCtrl+Shift+A");
    edit->Append(ID_SELECT_GA, "Select &Guiding Assistant Run",
                 "Select the frames of a Guiding Assistant run, with guide output off, to analyse them");
    edit->AppendSeparator();
    edit->Append(ID_EXCLUDE_SELECTION, "E&xclude Selected Frames\tCtrl+Back");
    edit->Append(ID_INCLUDE_SELECTION, "I&nclude Selected Frames\tCtrl+Shift+Back");
    edit->AppendSeparator();
    edit->Append(ID_INCLUDE_ALL, "&Include All Frames");
    edit->Append(ID_EXCLUDE_ALL, "&Exclude All Frames");
    edit->Append(ID_REAPPLY_RULES, "&Reapply Exclusion Rules",
                 "Apply the exclusion rules again, undoing frames included or excluded by hand");
#ifndef __WXOSX__
    edit->AppendSeparator();
#endif
    edit->Append(wxID_PREFERENCES, "&Preferences...\tCtrl+,");

    auto view = new wxMenu;
    for (int i = 0; i < 4; i++)
        view->AppendRadioItem(ID_VIEW_TIMELINE + i, wxString::Format("%s\tCtrl+%d", VIEW_NAMES[i], i + 1));
    view->AppendSeparator();
    view->Append(ID_ZOOM_IN, "Zoom &In\tCtrl+=");
    view->Append(ID_ZOOM_OUT, "Zoom &Out\tCtrl+-");
    view->Append(ID_ZOOM_FIT, "Zoom to &Fit\tCtrl+0");
    view->Append(ID_ZOOM_SELECTION, "Zoom to Se&lection\tCtrl+Shift+=");
    view->AppendSeparator();
    view->Append(ID_FIT_VERTICAL, "Fit &Vertical Scale\tCtrl+Shift+0");
    view->AppendCheckItem(ID_LOCK_VERTICAL, "Loc&k Vertical Scale",
                          "Use the same vertical scale, in arc-seconds, for every session");
    view->AppendSeparator();
    view->Append(ID_PREVIOUS_SECTION, "&Previous Section\tCtrl+Alt+Up");
    view->Append(ID_NEXT_SECTION, "&Next Section\tCtrl+Alt+Down");
    view->AppendSeparator();
#ifdef __WXOSX__
    view->AppendCheckItem(ID_TOGGLE_SIDEBAR, "Show &Sidebar\tCtrl+RawCtrl+S");
#else
    view->AppendCheckItem(ID_TOGGLE_SIDEBAR, "Show &Sidebar\tCtrl+Alt+S");
#endif
    view->AppendCheckItem(ID_TOGGLE_INSPECTOR, "Show &Inspector\tCtrl+Alt+I");

    auto help = new wxMenu;
    help->Append(wxID_HELP, APP_NAME " &Help");
    help->Append(wxID_ABOUT, "&About " APP_NAME);

    auto bar = new wxMenuBar;
    bar->Append(file, "&File");
    bar->Append(edit, "&Edit");
    bar->Append(view, "&View");
#ifdef __WXOSX__
    auto window = new wxMenu;
    window->Append(wxID_ICONIZE_FRAME, "Minimize\tCtrl+M");
    window->Append(wxID_MAXIMIZE_FRAME, "Zoom");
    bar->Append(window, "&Window");
#endif
    bar->Append(help, "&Help");
    SetMenuBar(bar);

    wxConfigBase *config = wxConfig::Get();
    m_history.UseMenu(recent);
    config->SetPath("/recent");
    m_history.Load(*config);
    config->SetPath("/");

    Bind(wxEVT_MENU, &MainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnOpenRecent, this, wxID_FILE1, wxID_FILE9);
    Bind(wxEVT_MENU, &MainFrame::OnOpenInEditor, this, ID_OPEN_IN_EDITOR);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_CLOSE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnCopy, this, wxID_COPY);
    Bind(wxEVT_MENU, &MainFrame::OnIncludeAll, this, ID_INCLUDE_ALL, ID_REAPPLY_RULES);
    Bind(wxEVT_MENU, &MainFrame::OnPreferences, this, wxID_PREFERENCES);
    Bind(wxEVT_MENU, &MainFrame::OnHelp, this, wxID_HELP);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, [this](wxCommandEvent& e) { SetView((ChartKind) (e.GetId() - ID_VIEW_TIMELINE)); },
         ID_VIEW_TIMELINE, ID_VIEW_FREQUENCY);
    Bind(wxEVT_MENU, &MainFrame::OnSelectAll, this, wxID_SELECTALL);
    Bind(wxEVT_MENU, &MainFrame::OnExportCsv, this, ID_EXPORT_CSV);
    Bind(wxEVT_MENU, [this](wxCommandEvent& e) {
        if (!m_doc.Current())
            return;
        size_t current = *m_doc.Current();
        if (e.GetId() == ID_PREVIOUS_SECTION && current > 0)
            SelectSection(current - 1);
        else if (e.GetId() == ID_NEXT_SECTION && current + 1 < m_doc.Log().sections.size())
            SelectSection(current + 1);
    }, ID_PREVIOUS_SECTION, ID_NEXT_SECTION);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        auto current = m_doc.Current();
        e.Enable(current && (e.GetId() == ID_PREVIOUS_SECTION ? *current > 0
                                                              : *current + 1 < m_doc.Log().sections.size()));
    }, ID_PREVIOUS_SECTION, ID_NEXT_SECTION);
    Bind(wxEVT_MENU, &MainFrame::OnSelectGuidingAssistant, this, ID_SELECT_GA);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        e.Enable(m_settings.view == ChartKind::Frequency && Analysis() != nullptr);
    }, ID_EXPORT_CSV);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        e.Enable(m_doc.Session() && !GuidingAssistantRuns(*m_doc.Session()).empty());
    }, ID_SELECT_GA);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        m_doc.SetSelection(std::nullopt);
        SelectionChanged();
    }, ID_DESELECT);
    Bind(wxEVT_MENU, &MainFrame::OnIncludeSelection, this, ID_EXCLUDE_SELECTION, ID_INCLUDE_SELECTION);
    Bind(wxEVT_MENU, &MainFrame::OnZoom, this, ID_ZOOM_IN, ID_FIT_VERTICAL);
    Bind(wxEVT_MENU, [this](wxCommandEvent& e) { LockVertical(e.IsChecked()); }, ID_LOCK_VERTICAL);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { ShowSidebar(!m_outer->IsSplit()); }, ID_TOGGLE_SIDEBAR);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { ShowInspector(!m_inner->IsSplit()); }, ID_TOGGLE_INSPECTOR);

    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(!m_path.empty()); }, ID_OPEN_IN_EDITOR);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(m_doc.Session() != nullptr); },
         ID_INCLUDE_ALL, ID_REAPPLY_RULES);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        e.Check(e.GetId() - ID_VIEW_TIMELINE == (int) m_settings.view);
    }, ID_VIEW_TIMELINE, ID_VIEW_FREQUENCY);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(m_doc.Selection().has_value()); },
         ID_DESELECT, ID_INCLUDE_SELECTION);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        e.Enable(m_doc.Selection().has_value() && m_settings.view == ChartKind::Timeline);
    }, ID_ZOOM_SELECTION);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        Chart *chart = m_doc.Current() ? m_chart->ActiveChart() : nullptr;
        switch (e.GetId())
        {
        case ID_ZOOM_IN:
            e.Enable(chart && chart->CanZoomIn());
            break;
        case ID_ZOOM_OUT:
        case ID_ZOOM_FIT:
            e.Enable(chart && chart->CanZoomOut());
            break;
        }
    }, ID_ZOOM_IN, ID_ZOOM_FIT);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        e.Enable(m_doc.Session() && m_settings.view == ChartKind::Timeline);
    }, ID_FIT_VERTICAL);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        e.Enable(m_doc.Session() != nullptr);
        e.Check(m_doc.VerticalScaleLocked());
    }, ID_LOCK_VERTICAL);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(m_doc.Session() != nullptr); }, wxID_SELECTALL);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Check(m_outer->IsSplit()); }, ID_TOGGLE_SIDEBAR);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Check(m_inner->IsSplit()); }, ID_TOGGLE_INSPECTOR);
}

void MainFrame::BuildToolbar()
{
    wxToolBar *tb = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER, wxID_ANY, "Toolbar");
    tb->AddTool(wxID_OPEN, "Open", ToolIcon(Icon::Open, m_palette.text2), "Open a guide log");
    tb->AddTool(ID_TOGGLE_SIDEBAR, "Sidebar", ToolIcon(Icon::Sidebar, m_palette.text2), "Show or hide the session list");
    tb->AddStretchableSpace();
    m_views = new SegmentedControl(tb, wxID_ANY, { VIEW_NAMES[0], VIEW_NAMES[1], VIEW_NAMES[2], VIEW_NAMES[3] }, "View");
    m_views->SetToolTip("Choose what the chart shows");
    tb->AddControl(m_views, "View");
    tb->AddStretchableSpace();
    tb->AddTool(ID_TOGGLE_INSPECTOR, "Inspector", ToolIcon(Icon::Inspector, m_palette.text2),
                "Show or hide the inspector");
    tb->Realize();

    m_views->Bind(wxEVT_CHOICE, [this](wxCommandEvent& e) { SetView((ChartKind) e.GetInt()); });
}

void MainFrame::UpdateToolIcons()
{
    wxToolBar *tb = GetToolBar();
    tb->SetToolNormalBitmap(wxID_OPEN, ToolIcon(Icon::Open, m_palette.text2));
    tb->SetToolNormalBitmap(ID_TOGGLE_SIDEBAR, ToolIcon(Icon::Sidebar, m_palette.text2));
    tb->SetToolNormalBitmap(ID_TOGGLE_INSPECTOR, ToolIcon(Icon::Inspector, m_palette.text2));
}

void MainFrame::BuildLayout()
{
    long style = wxSP_LIVE_UPDATE | wxSP_NOBORDER | wxSP_THIN_SASH;
    m_outer = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, style);
    m_outer->SetSashGravity(0.);
    m_outer->SetMinimumPaneSize(FromDIP(170));

    m_sidebar = new SessionSidebar(m_outer);
    m_inner = new wxSplitterWindow(m_outer, wxID_ANY, wxDefaultPosition, wxDefaultSize, style);
    m_inner->SetSashGravity(1.);
    m_inner->SetMinimumPaneSize(FromDIP(220));

    m_chart = new ChartView(m_inner, *this);
    m_inspector = new Inspector(m_inner);

    // the panes are split once the frame has its size
    m_sidebar->Hide();
    m_inspector->Hide();
    m_outer->Initialize(m_inner);
    m_inner->Initialize(m_chart);

    m_sidebar->onSelect = [this](size_t section) { SelectSection(section); };
    m_inspector->onDisplay = [this](const DisplayOptions& options) { SetDisplay(options); };
    m_inspector->onLockVertical = [this](bool lock) { LockVertical(lock); };
    m_inspector->onAnalysis = [this](const AnalysisOptions& options) {
        m_settings.analysis = options;
        UpdateAll();
    };
    m_inspector->onSelectGuidingAssistant = [this] {
        wxCommandEvent dummy;
        OnSelectGuidingAssistant(dummy);
    };
    m_inspector->onExclusions = [this](const ExclusionSettings& rules) {
        ExclusionSettings x = rules;
        x.settle = m_settings.exclusions.settle;
        m_settings.exclusions = x;
        m_doc.SetExclusions(x);
        UpdateAll();
    };
    m_inspector->onIncludeAll = [this](bool include) {
        m_doc.IncludeAll(include);
        UpdateAll();
    };

    // let a log be dropped on the chart too
    for (wxWindow *w : m_chart->DropTargets())
        w->SetDropTarget(new FileDropTarget(this));
}

void MainFrame::ApplyPalette()
{
    m_palette = CurrentPalette(m_settings.raColor, m_settings.decColor);
    m_sidebar->SetPalette(m_palette);
    m_inspector->SetPalette(m_palette);
    m_chart->ApplyPalette();
    m_views->SetPalette(m_palette);
    UpdateToolIcons();
    if (m_helpWindow)
        for (wxWindow *child : m_helpWindow->GetChildren())
            if (auto html = dynamic_cast<wxHtmlWindow *>(child))
                html->SetPage(HelpHtml(m_palette));
}

void MainFrame::OnSysColourChanged(wxSysColourChangedEvent& event)
{
    ApplyPalette();
    event.Skip();
}

void MainFrame::Open(const wxString& path)
{
    std::ifstream ifs(path.fn_str());
    if (!ifs)
    {
        wxLogError("Cannot open the file \"%s\".", path);
        return;
    }

    wxFileName fn(path);
    bool ok;
    {
        wxWindowDisabler disabler;
        wxBusyInfo busy(wxString::Format("Reading %s...", fn.GetFullName()), this);
        ok = m_doc.Load(ifs, [] { wxTheApp->Yield(true); });
    }
    if (!ok)
    {
        wxLogError("Cannot read the file \"%s\".", path);
        return;
    }

    m_path = path;
    m_history.AddFileToHistory(path);
    m_settings.openDir = fn.GetPath();

    // start on the first guiding session, the usual reason to open a log
    const GuideLog& log = m_doc.Log();
    for (size_t i = 0; i < log.sections.size(); i++)
        if (log.sections[i].type == GUIDING_SECTION)
        {
            m_doc.Select(i);
            break;
        }
    ChooseDevice();

    UpdateAll();
    // the arrow keys move between sections straight away
    m_sidebar->FocusList();

    if (log.is_debug_log)
    {
        wxMessageBox(wxString::Format(
                         "%s is a PHD2 debug log.\n\n"
                         APP_NAME " reads guide logs. Open the file named "
                         "PHD2_GuideLog_<date>.txt from the same folder instead.",
                         fn.GetFullName()),
                     "Not a guide log", wxOK | wxICON_INFORMATION, this);
    }
}

void MainFrame::OnOpen(wxCommandEvent&)
{
    wxFileDialog dlg(this, "Open a PHD2 Guide Log", m_settings.openDir, wxEmptyString,
                     "PHD2 guide logs (*PHD2_GuideLog*.txt)|*PHD2_GuideLog*.txt|Text files (*.txt)|*.txt",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK)
        Open(dlg.GetPath());
}

void MainFrame::OnOpenRecent(wxCommandEvent& event)
{
    wxString path = m_history.GetHistoryFile(event.GetId() - wxID_FILE1);
    if (!wxFileName::FileExists(path))
    {
        wxLogError("The file \"%s\" is no longer there.", path);
        m_history.RemoveFileFromHistory(event.GetId() - wxID_FILE1);
        return;
    }
    Open(path);
}

void MainFrame::OnOpenInEditor(wxCommandEvent&)
{
    if (!m_path.empty())
        wxLaunchDefaultApplication(m_path);
}

void MainFrame::OnCopy(wxCommandEvent& event)
{
    // text the user selected, such as in the raw header, copies as text
    if (auto text = dynamic_cast<wxTextCtrl *>(FindFocus()); text && text->CanCopy())
    {
        text->Copy();
        return;
    }

    const GuideSession *session = m_doc.Session();
    if (!session)
    {
        event.Skip();
        return;
    }
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(U(StatsCsv(m_doc.Stats(), session->pixelScale))));
        wxTheClipboard->Close();
    }
}

void MainFrame::OnIncludeAll(wxCommandEvent& event)
{
    switch (event.GetId())
    {
    case ID_INCLUDE_ALL:
        m_doc.IncludeAll(true);
        break;
    case ID_EXCLUDE_ALL:
        m_doc.IncludeAll(false);
        break;
    case ID_REAPPLY_RULES:
        m_doc.SetExclusions(m_settings.exclusions);
        break;
    }
    UpdateAll();
}

void MainFrame::OnPreferences(wxCommandEvent&)
{
    if (!m_preferences)
    {
        m_preferences = CreatePreferences(m_settings, [this] {
            SettleParams before = m_doc.Exclusions().settle;
            const SettleParams& after = m_settings.exclusions.settle;
            if (before.pixels != after.pixels || before.seconds != after.seconds)
                m_doc.SetExclusions(m_settings.exclusions);
            ApplyPalette();
            UpdateAll();
        });
    }
    m_preferences->Show(this);
}

void MainFrame::OnHelp(wxCommandEvent&)
{
    if (!m_helpWindow)
    {
        auto frame = new wxFrame(this, wxID_ANY, APP_NAME " Help", wxDefaultPosition, FromDIP(wxSize(560, 520)),
                                 wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT);
        auto html = new wxHtmlWindow(frame, wxID_ANY);
        html->SetPage(HelpHtml(m_palette));
        frame->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
            m_helpWindow = nullptr;
            e.Skip();
        });
        m_helpWindow = frame;
    }
    m_helpWindow->Show();
    m_helpWindow->Raise();
}

void MainFrame::OnAbout(wxCommandEvent&)
{
    wxAboutDialogInfo info;
    info.SetName(APP_NAME);
    info.SetVersion(APP_VERSION_STR);
    info.SetDescription("See and analyze your PHD2 guide logs.\nBased on PHD2 Log Viewer by Andy Galasso.");
    info.SetCopyright("(C) 2016-2020 Andy Galasso\n(C) 2026 OpenAstro contributors");
    info.SetWebSite("https://github.com/open-astro/openastro-guide-insight");
    info.SetLicence("GNU General Public License, version 3 or later");
    info.AddDeveloper("Andy Galasso");
    info.AddDeveloper("OpenAstro contributors");
    wxAboutBox(info, this);
}

void MainFrame::SelectSection(size_t section)
{
    m_doc.Select(section);
    ChooseDevice();
    m_chart->Timeline()->ClearHover();
    UpdateAll();
}

// Shows the pulses of the device a session guided with. Only a session
// that used both an AO and a mount offers the choice.
void MainFrame::ChooseDevice()
{
    const GuideSession *s = m_doc.Session();
    if (!s || HasBothDevices(*s))
        return;
    bool ao = std::any_of(s->entries.begin(), s->entries.end(), [](const GuideEntry& e) { return e.mount == AO; });
    m_settings.display.device = ao ? AO : MOUNT;
}

void MainFrame::SetDisplay(const DisplayOptions& options)
{
    if (options.xaxis != m_settings.display.xaxis)
        m_doc.SetXAxis(options.xaxis);
    m_settings.display = options;
    UpdateAll();
}

void MainFrame::LockVertical(bool lock)
{
    m_doc.LockVerticalScale(lock);
    m_settings.verticalLock = m_doc.VerticalLock();
    m_inspector->ShowDocument(m_doc, m_settings);
    m_chart->RefreshChart();
}

void MainFrame::OnSelectAll(wxCommandEvent& event)
{
    if (auto text = dynamic_cast<wxTextCtrl *>(FindFocus()))
    {
        text->SelectAll();
        return;
    }
    const GuideSession *s = m_doc.Session();
    if (!s || s->entries.empty())
    {
        event.Skip();
        return;
    }
    m_doc.SetSelection(IndexRun{ 0, (unsigned int) s->entries.size() - 1 });
    SelectionChanged();
}

void MainFrame::OnIncludeSelection(wxCommandEvent& event)
{
    m_doc.IncludeSelection(event.GetId() == ID_INCLUDE_SELECTION);
    FramesChanged();
}

void MainFrame::OnZoom(wxCommandEvent& event)
{
    TimelineCanvas *t = m_chart->Timeline();
    Chart *chart = m_chart->ActiveChart();
    if (!chart)
        return;
    switch (event.GetId())
    {
    case ID_ZOOM_IN:
        chart->ZoomBy(2.);
        break;
    case ID_ZOOM_OUT:
        chart->ZoomBy(0.5);
        break;
    case ID_ZOOM_FIT:
        chart->ZoomToFit();
        break;
    case ID_ZOOM_SELECTION:
        t->ZoomToSelection();
        break;
    case ID_FIT_VERTICAL:
        t->FitVertical();
        break;
    }
}

void MainFrame::FramesChanged()
{
    UpdateAll();
}

void MainFrame::SelectionChanged()
{
    m_inspector->ShowDocument(m_doc, m_settings);
    if (m_settings.view != ChartKind::Timeline)
        UpdateChart();
    m_chart->RefreshChart();
}

void MainFrame::ViewChanged()
{
    UpdateSubtitle();
    m_chart->RefreshChart();
}

void MainFrame::ShowStatus(const wxString& text)
{
    m_chart->ShowStatus(text);
}

void MainFrame::UpdateSubtitle()
{
    const GuideSession *s = m_doc.Session();
    SessionView *v = m_chart->Timeline()->View();
    if (!s || !v || m_settings.view != ChartKind::Timeline || s->entries.empty())
        return;
    const auto& e = s->entries;
    XAxis axis = m_doc.GetXAxis();
    double t0 = PositionAtIndex(e, IndexAtPosition(e, v->horizontal.First(), axis), XAxis::Time);
    double t1 = PositionAtIndex(e, IndexAtPosition(e, v->horizontal.Last(), axis), XAxis::Time);
    m_chart->SetHeading(U(SectionTitle(m_doc.Log(), *m_doc.Current())), U(VisibleRange(*s, t0, t1)));
}

void MainFrame::SetView(ChartKind view)
{
    m_settings.view = view;
    m_views->SetSelection((int) view);
    m_inspector->ShowDocument(m_doc, m_settings);
    UpdateChart();
}

const GARun *MainFrame::Analysis()
{
    const GuideSession *s = m_doc.Session();
    if (!s || s->entries.empty())
        return nullptr;

    AnalysisKey key;
    key.session = s;
    key.range = m_doc.Selection() ? *m_doc.Selection() : IndexRun{ 0, (unsigned int) s->entries.size() - 1 };
    key.options = m_settings.analysis;
    if (m_analysisStale || !(key == m_analysisKey))
    {
        m_analysisStale = false;
        m_analysisKey = key;
        m_analysisGeneration++;
        m_analysis.reset();
        size_t begin = key.range.first, end = key.range.last + 1;
        if (GARun::CanAnalyze(*s, begin, end))
        {
            m_analysis = std::make_unique<GARun>();
            m_analysis->Analyze(*s, begin, end, key.options.rawRa, key.options.driftCorrection);
        }
    }
    return m_analysis.get();
}

wxString MainFrame::AnalysisScope() const
{
    const auto& sel = m_doc.Selection();
    return sel ? wxString::Format("frames %u to %u", sel->first + 1, sel->last + 1) : wxString("whole session");
}

void MainFrame::OnExportCsv(wxCommandEvent&)
{
    const GARun *ga = Analysis();
    if (!ga || ga->nfft == 0)
        return;
    wxFileDialog dlg(this, "Export Periodogram as CSV", m_settings.openDir,
                     wxString::Format("periodogram_%s.csv", ga->starts.Format("%Y%m%d_%H%M%S")),
                     "CSV files (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK)
        return;
    wxFile f;
    if (!f.Create(dlg.GetPath(), true) || !f.Write(wxString::FromUTF8(FftCsv(*ga))))
        wxLogError("Cannot write the file \"%s\".", dlg.GetPath());
}

// Selects a Guiding Assistant run: the first, or the next after the one
// already selected.
void MainFrame::OnSelectGuidingAssistant(wxCommandEvent&)
{
    const GuideSession *s = m_doc.Session();
    if (!s)
        return;
    std::vector<IndexRun> runs = GuidingAssistantRuns(*s);
    if (runs.empty())
        return;
    IndexRun next = runs.front();
    if (const auto& sel = m_doc.Selection())
        for (const IndexRun& r : runs)
            if (r.first > sel->first)
            {
                next = r;
                break;
            }
    m_doc.SetSelection(next);
    SelectionChanged();
}

void MainFrame::ShowSidebar(bool show)
{
    if (show == m_outer->IsSplit())
        return;
    if (show)
    {
        m_outer->SplitVertically(m_sidebar, m_inner, FromDIP(m_settings.sidebarWidth));
        m_sidebar->FocusList();
    }
    else
    {
        m_settings.sidebarWidth = ToDIP(m_outer->GetSashPosition());
        m_outer->Unsplit(m_sidebar);
    }
}

void MainFrame::ShowInspector(bool show)
{
    if (show == m_inner->IsSplit())
        return;
    if (show)
        m_inner->SplitVertically(m_chart, m_inspector, -FromDIP(m_settings.inspectorWidth));
    else
    {
        m_settings.inspectorWidth = ToDIP(m_inner->GetSize().x - m_inner->GetSashPosition());
        m_inner->Unsplit(m_inspector);
    }
}

void MainFrame::UpdateAll()
{
    m_analysisStale = true;
    m_sidebar->SetLog(m_doc.Log(), m_doc.Current(), m_settings.display.units);
    m_inspector->ShowDocument(m_doc, m_settings);
    UpdateTitle();
    UpdateChart();
}

void MainFrame::UpdateTitle()
{
    if (m_path.empty())
    {
        SetTitle(APP_NAME);
        return;
    }
    wxString title = wxFileName(m_path).GetFullName();
#ifndef __WXOSX__
    title += wxString::FromUTF8(" — " APP_NAME);
#endif
    SetTitle(title);
#ifdef __WXOSX__
    SetRepresentedFilename(m_path);
#endif
}

void MainFrame::UpdateChart()
{
    if (!m_doc.Current())
    {
        m_chart->SetHeading(wxEmptyString, wxEmptyString);
        m_chart->SetLegend(wxEmptyString, wxEmptyString);
        m_chart->SetDefaultStatus(wxEmptyString);
        if (m_path.empty())
            m_chart->ShowMessage("Open a PHD2 guide log", wxString::FromUTF8("Choose File › Open, or drag a log file here"));
        else
            m_chart->ShowMessage("This log has no calibrations or guiding");
        return;
    }

    size_t section = *m_doc.Current();
    m_chart->SetHeading(U(SectionTitle(m_doc.Log(), section)), wxEmptyString);
    if (m_doc.CalibrationSection())
    {
        m_chart->SetHeading(U(SectionTitle(m_doc.Log(), section)), "Camera pixels from the start");
        m_chart->SetLegend("West", "North");
        m_chart->SetDefaultStatus(wxString::FromUTF8(
#ifdef __WXOSX__
            "Drag to pan · pinch or ⌘-scroll to zoom · point at a step to see it"
#else
            "Drag to pan · Ctrl-scroll to zoom · point at a step to see it"
#endif
            ));
        m_chart->ShowCalibration();
        return;
    }

    bool radec = m_settings.display.raDecAxes;
    m_chart->SetLegend(radec ? "RA" : "dx", radec ? "Dec" : "dy");
    if (m_settings.view == ChartKind::Scatter)
    {
        const GuideSession *s = m_doc.Session();
        size_t frames = m_doc.Selection() ? m_doc.Selection()->last - m_doc.Selection()->first + 1 : s->entries.size();
        m_chart->SetHeading(U(SectionTitle(m_doc.Log(), section)),
                            wxString::FromUTF8(std::string(radec ? "RA against Dec" : "dx against dy") +
                                               (m_doc.Selection() ? " \u00b7 selected frames" : " \u00b7 whole session")));
        m_chart->SetLegend(wxEmptyString, wxEmptyString);
        m_chart->SetDefaultStatus(wxString::FromUTF8(Summarize(m_doc.Stats(), frames, s->pixelScale,
                                                                m_settings.display.units).frames +
                                                      (radec ? " \u00b7 the ellipse spans two standard deviations" : "")));
        m_chart->ShowScatter();
        return;
    }
    if (m_settings.view == ChartKind::Drift || m_settings.view == ChartKind::Frequency)
    {
        bool drift = m_settings.view == ChartKind::Drift;
        const AnalysisOptions& a = m_settings.analysis;
        wxString what = drift ? (a.driftCorrection ? "Drift removed" : "Drift not removed") : "RA periodogram";
        if (a.rawRa)
            what += ", RA pulses added back";
        m_chart->SetHeading(U(SectionTitle(m_doc.Log(), section)), what + wxString::FromUTF8(" \u00b7 ") + AnalysisScope());
        if (!drift)
            m_chart->SetLegend(wxEmptyString, wxEmptyString);
        if (!Analysis())
        {
            m_chart->SetDefaultStatus(wxEmptyString);
            m_chart->ShowMessage("Too few frames to analyse",
                                 "Drift and Frequency need at least 12 frames that count toward the statistics");
            return;
        }
        m_chart->SetDefaultStatus(wxString::FromUTF8(drift ?
#ifdef __WXOSX__
            "Drag or scroll to pan · pinch or ⌘-scroll to zoom · select frames in the Timeline to analyse them"
            : "Point at the curve to read a peak · File › Export Periodogram as CSV saves it"
#else
            "Drag or scroll to pan · Ctrl-scroll to zoom · select frames in the Timeline to analyse them"
            : "Point at the curve to read a peak · File › Export Periodogram as CSV saves it"
#endif
            ));
        if (drift)
            m_chart->ShowDrift();
        else
            m_chart->ShowFrequency();
        return;
    }

    m_chart->SetDefaultStatus(wxString::FromUTF8(
#ifdef __WXOSX__
        "Drag to select · ⌘-drag to exclude · ⇧-drag to include · pinch to zoom"
#else
        "Drag to select · Ctrl-drag to exclude · Shift-drag to include · Ctrl-scroll to zoom"
#endif
        ));
    m_chart->ShowTimeline();
    UpdateSubtitle();
}

void MainFrame::SaveLayout()
{
    if (m_outer->IsSplit())
        m_settings.sidebarWidth = ToDIP(m_outer->GetSashPosition());
    if (m_inner->IsSplit())
        m_settings.inspectorWidth = ToDIP(m_inner->GetSize().x - m_inner->GetSashPosition());
    m_settings.sidebarShown = m_outer->IsSplit();
    m_settings.inspectorShown = m_inner->IsSplit();
    m_settings.verticalLock = m_doc.VerticalLock();

    if (!IsFullScreen())
    {
        Geometry g;
        g.maximized = IsMaximized();
        wxRect r = GetRect();
        g.width = r.width, g.height = r.height, g.x = r.x, g.y = r.y;
        m_settings.geometry = g;
    }
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    SaveLayout();
    wxConfigBase *config = wxConfig::Get();
    SaveSettings(*config, m_settings);
    config->SetPath("/recent");
    m_history.Save(*config);
    config->SetPath("/");
    config->Flush();

    if (m_helpWindow)
        m_helpWindow->Destroy();
    event.Skip();
}
