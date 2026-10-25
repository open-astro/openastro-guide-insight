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

#ifndef LOGVIEWAPP_INCLUDED
#define LOGVIEWAPP_INCLUDED

#include <wx/app.h>

class MainFrame;

class LogViewApp : public wxApp
{
    MainFrame *m_frame = nullptr;
    wxArrayString m_openFiles;
    bool m_printVersion = false;

public:
    LogViewApp();

private:
    bool OnInit() override;
    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;
#ifdef __WXOSX__
    void MacOpenFiles(const wxArrayString& files) override;
#endif
};

wxDECLARE_APP(LogViewApp);

#endif
