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
#include "icons.h"
#include "labels.h"

#include <wx/aboutdlg.h>
#include <wx/busyinfo.h>
#include <wx/clipbrd.h>
#include <wx/config.h>
#include <wx/display.h>
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
};

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

const char *HELP_HTML =
    "<html><body>"
    "<h3>Opening a guide log</h3>"
    "<p>Choose <b>File &rsaquo; Open</b>, pick one from <b>Open Recent</b>, or drag a "
    "PHD2_GuideLog file onto the window. You can also open one from the command line.</p>"
    "<h3>The window</h3>"
    "<p>The <b>sidebar</b> lists the log's calibrations and guiding sessions. Choose one to show it.</p>"
    "<p>The <b>toolbar</b> switches the chart between Timeline, Scatter, Drift and Frequency "
    "(&#8984;1 to &#8984;4 on a Mac, Ctrl+1 to Ctrl+4 elsewhere).</p>"
    "<p>The <b>inspector</b> on the right shows the statistics, the session's equipment and settings, "
    "display options and the exclusion rules. Hide either side from the View menu.</p>"
    "<h3>Statistics</h3>"
    "<p>Statistics count the frames where the star was found and that are not excluded. "
    "<b>Edit &rsaquo; Copy Statistics</b> copies them as CSV.</p>"
    "<p>Exclusion rules in the inspector leave out frames while PHD2 settles after a dither, "
    "and while a client has paused guiding. The distance rule's settings are in Preferences.</p>"
    "</body></html>";

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
    file->AppendSeparator();
    file->Append(wxID_CLOSE, "&Close Window\tCtrl+W");
#ifndef __WXOSX__
    file->Append(wxID_EXIT, "E&xit");
#endif

    auto edit = new wxMenu;
    edit->Append(wxID_COPY, "&Copy Statistics\tCtrl+C", "Copy the statistics as CSV");
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
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { ShowSidebar(!m_outer->IsSplit()); }, ID_TOGGLE_SIDEBAR);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { ShowInspector(!m_inner->IsSplit()); }, ID_TOGGLE_INSPECTOR);

    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(!m_path.empty()); }, ID_OPEN_IN_EDITOR);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(m_doc.Session() != nullptr); },
         ID_INCLUDE_ALL, ID_REAPPLY_RULES);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        e.Check(e.GetId() - ID_VIEW_TIMELINE == (int) m_settings.view);
    }, ID_VIEW_TIMELINE, ID_VIEW_FREQUENCY);
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

    m_chart = new ChartView(m_inner);
    m_inspector = new Inspector(m_inner);

    // the panes are split once the frame has its size
    m_sidebar->Hide();
    m_inspector->Hide();
    m_outer->Initialize(m_inner);
    m_inner->Initialize(m_chart);

    m_sidebar->onSelect = [this](size_t section) { SelectSection(section); };
    m_inspector->onUnits = [this](Units units) {
        m_settings.display.units = units;
        UpdateAll();
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
    m_chart->Canvas()->SetDropTarget(new FileDropTarget(this));
}

void MainFrame::ApplyPalette()
{
    m_palette = CurrentPalette(m_settings.raColor, m_settings.decColor);
    m_sidebar->SetPalette(m_palette);
    m_inspector->SetPalette(m_palette);
    m_chart->SetPalette(m_palette);
    m_views->SetPalette(m_palette);
    UpdateToolIcons();
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

    UpdateAll();

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
        html->SetPage(HELP_HTML);
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
    UpdateAll();
}

void MainFrame::SetView(ChartKind view)
{
    m_settings.view = view;
    m_views->SetSelection((int) view);
    UpdateChart();
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
        m_chart->SetStatus(wxEmptyString);
        if (m_path.empty())
            m_chart->SetMessage("Open a PHD2 guide log", wxString::FromUTF8("Choose File › Open, or drag a log file here"));
        else
            m_chart->SetMessage("This log has no calibrations or guiding");
        return;
    }

    size_t section = *m_doc.Current();
    m_chart->SetHeading(U(SectionTitle(m_doc.Log(), section)), wxEmptyString);
    if (m_doc.CalibrationSection())
    {
        m_chart->SetMessage("Calibration");
        m_chart->SetStatus(wxEmptyString);
        return;
    }
    m_chart->SetMessage(VIEW_NAMES[(int) m_settings.view]);
    m_chart->SetStatus(wxString::Format("%zu frames", m_doc.Session()->entries.size()));
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
