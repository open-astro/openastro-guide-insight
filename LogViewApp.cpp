/*
 * This file is part of OpenAstro Guide Insight, a fork of PHD2 Log Viewer
 *
 * Copyright (C) 2016 Andy Galasso
 * Copyright (C) 2026 Open Astro contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, visit the http://fsf.org website.
 */

#include "LogViewApp.h"
#include "MainFrame.h"
#include "settingsmigration.h"
#include "version.h"

#include <gsl/gsl_errno.h>
#include <wx/cmdline.h>
#include <wx/config.h>
#include <wx/log.h>

#include <cstdio>
#include <cstdlib>

static void gsl_error_handler(const char *WXUNUSED(reason), const char *WXUNUSED(file), int WXUNUSED(line), int WXUNUSED(gsl_errno))
{
#if defined(_MSC_VER)
    DebugBreak();
#endif
}

LogViewApp::LogViewApp()
{
    SetVendorName("OpenAstro");
    SetAppName("guide-insight");
    SetAppDisplayName("OpenAstro Guide Insight");

    // Carry over settings saved before the app was renamed from PHD2 Log Viewer.
    wxConfig previous("phdlogview", "adgsoftware");
    MigrateSettings(previous, *wxConfig::Get());
}

bool LogViewApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    if (m_printVersion)
    {
        // for scripts, and the packaging checks
        printf("%s %s\n", GetAppDisplayName().utf8_str().data(), GUIDEINSIGHT_VERSION);
        fflush(stdout);
        std::exit(EXIT_SUCCESS);
    }

    wxLog::SetActiveTarget(new wxLogStderr());
    gsl_set_error_handler(&gsl_error_handler);

    m_frame = new MainFrame();
    m_frame->Show();

    if (!m_openFile.IsEmpty())
        m_frame->Open(m_openFile);

    return true;
}

void LogViewApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    parser.AddSwitch(wxEmptyString, "version", "Print the version and exit");
    parser.AddParam("filename", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
}

bool LogViewApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    if (parser.GetParamCount() > 1)
        return false;
    m_printVersion = parser.Found("version");
    if (parser.GetParamCount() == 1)
        m_openFile = parser.GetParam(0);
    return true;
}

#ifdef __WXOSX__
// Files opened from the Finder, or dropped on the Dock icon.
void LogViewApp::MacOpenFiles(const wxArrayString& files)
{
    if (files.empty())
        return;
    if (m_frame)
        m_frame->Open(files[0]);
    else
        m_openFile = files[0];
}
#endif

wxIMPLEMENT_APP(LogViewApp);
