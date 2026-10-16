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

#ifndef INSPECTOR_INCLUDED
#define INSPECTOR_INCLUDED

// The column of collapsible sections down the right of the window: the
// statistics, session details, display options and exclusion rules.

#include "appsettings.h"
#include "document.h"
#include "theme.h"

#include <wx/scrolwin.h>

#include <functional>
#include <vector>

class SectionHeader;
class SegmentedControl;
class wxButton;
class wxCheckBox;
class wxFlexGridSizer;
class wxPanel;
class wxStaticText;
class wxTextCtrl;

class Inspector : public wxScrolledWindow
{
    // A heading and the panel it opens and closes.
    struct Section
    {
        SectionHeader *header;
        wxPanel *body;
        void Show(bool show);
    };

    Palette m_palette;
    wxStaticText *m_empty;

    Section m_summary;
    wxStaticText *m_total, *m_alt, *m_ra, *m_dec, *m_peak, *m_elongation, *m_frames;

    Section m_drift;
    wxStaticText *m_raDrift, *m_decDrift, *m_paError;

    Section m_details;
    wxFlexGridSizer *m_detailsGrid;
    SectionHeader *m_rawHeader;
    wxTextCtrl *m_raw;

    Section m_display;
    SegmentedControl *m_units;

    Section m_exclusions;
    wxStaticText *m_excluded;
    wxCheckBox *m_byServer, *m_byDistance, *m_pauses;
    wxButton *m_includeAll, *m_excludeAll;

    std::vector<wxStaticText *> m_secondary;  // text in the secondary colour
    std::vector<wxStaticText *> m_labels;     // field labels, all one width
    std::vector<SectionHeader *> m_headers;

    Section AddSection(const wxString& title, bool open = true);
    wxStaticText *AddField(wxWindow *parent, wxFlexGridSizer *grid, const wxString& label);
    wxStaticText *Secondary(wxStaticText *text);
    void AlignLabels();
    void Relayout();
    ExclusionSettings Rules() const;

public:
    explicit Inspector(wxWindow *parent);

    std::function<void(Units)> onUnits;
    std::function<void(const ExclusionSettings&)> onExclusions;
    std::function<void(bool include)> onIncludeAll;

    // Shows what the document's current section and selection hold.
    void ShowDocument(const Document& doc, const AppSettings& settings);
    void SetPalette(const Palette& palette);
};

#endif
