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

#include "Inspector.h"

#include "SectionHeader.h"
#include "SegmentedControl.h"
#include "labels.h"
#include "sessiondetails.h"
#include "timeline.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/wupdlock.h>

#include <algorithm>
#include <initializer_list>
#include <vector>

namespace {

wxString U(const std::string& utf8)
{
    return wxString::FromUTF8(utf8);
}

wxFlexGridSizer *FieldGrid()
{
    auto grid = new wxFlexGridSizer(2, wxSize(12, 4));
    grid->AddGrowableCol(1, 1);
    return grid;
}

} // namespace

void Inspector::Section::Show(bool show)
{
    header->Show(show);
    body->Show(show && header->IsOpen());
}

Inspector::Inspector(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE, "Inspector"),
      m_palette(CurrentPalette())
{
    SetScrollRate(0, FromDIP(10));
    auto sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(sizer);

    m_empty = Secondary(new wxStaticText(this, wxID_ANY, "No section selected"));
    sizer->Add(m_empty, 0, wxALL, FromDIP(14));

    // Summary
    m_summary = AddSection("Summary");
    {
        wxWindow *p = m_summary.body;
        auto s = new wxBoxSizer(wxVERTICAL);
        m_total = new wxStaticText(p, wxID_ANY, wxEmptyString);
        m_total->SetFont(GetFont().Scaled(2.f).Bold());
        m_alt = Secondary(new wxStaticText(p, wxID_ANY, wxEmptyString));
        s->Add(m_total);
        s->Add(m_alt, 0, wxBOTTOM, FromDIP(8));
        auto grid = FieldGrid();
        m_ra = AddField(p, grid, "RA RMS");
        m_dec = AddField(p, grid, "Dec RMS");
        m_peak = AddField(p, grid, "Peak");
        m_elongation = AddField(p, grid, "Elongation");
        s->Add(grid, 0, wxEXPAND);
        m_frames = Secondary(new wxStaticText(p, wxID_ANY, wxEmptyString));
        s->Add(m_frames, 0, wxTOP, FromDIP(6));
        p->SetSizer(s);
    }

    // Drift and polar alignment
    m_drift = AddSection("Drift & polar alignment");
    {
        wxWindow *p = m_drift.body;
        auto grid = FieldGrid();
        m_raDrift = AddField(p, grid, "RA drift");
        m_decDrift = AddField(p, grid, "Dec drift");
        m_paError = AddField(p, grid, "Polar alignment");
        m_paError->SetToolTip("Polar alignment error, estimated from the Dec drift");
        p->SetSizer(grid);
    }

    // Analysis, for the Drift and Frequency charts
    m_analysis = AddSection("Analysis");
    {
        wxWindow *p = m_analysis.body;
        auto s = new wxBoxSizer(wxVERTICAL);
        m_scope = Secondary(new wxStaticText(p, wxID_ANY, wxEmptyString));
        s->Add(m_scope, 0, wxBOTTOM, FromDIP(6));
        auto send = [this] {
            if (onAnalysis)
                onAnalysis(AnalysisOptions{ (DriftModel) m_driftModel->GetSelection(), m_rawRa->GetValue() });
        };
        auto row = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText *label = Secondary(new wxStaticText(p, wxID_ANY, "Remove drift"));
        row->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
        // in DriftModel's order: None, Line, Curve
        m_driftModel = new SegmentedControl(p, wxID_ANY, { "None", "Line", "Curve" }, "Remove drift");
        m_driftModel->SetToolTip("Take a straight-line or curved (quadratic) drift out of RA and Dec before analysing "
                            "them, or none to see the raw drift");
        m_driftModel->Bind(wxEVT_CHOICE, [send](wxCommandEvent&) { send(); });
        row->Add(m_driftModel);
        s->Add(row, 0, wxBOTTOM, FromDIP(8));
        m_rawRa = new wxCheckBox(p, wxID_ANY, "Add back RA guide pulses");
        m_rawRa->SetToolTip("Analyse RA as it would have moved without guiding, by adding the RA pulses back");
        m_rawRa->Bind(wxEVT_CHECKBOX, [send](wxCommandEvent&) { send(); });
        s->Add(m_rawRa, 0, wxBOTTOM, FromDIP(4));
        m_selectGa = new wxButton(p, wxID_ANY, "Select Guiding Assistant Run");
        m_selectGa->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            if (onSelectGuidingAssistant)
                onSelectGuidingAssistant();
        });
        s->Add(m_selectGa, 0, wxTOP, FromDIP(4));
        p->SetSizer(s);
    }

    // Calibration results
    m_calibration = AddSection("Calibration");
    {
        m_calibrationGrid = FieldGrid();
        m_calibration.body->SetSizer(m_calibrationGrid);
    }

    // Session details
    m_details = AddSection("Session details");
    {
        wxWindow *p = m_details.body;
        auto s = new wxBoxSizer(wxVERTICAL);
        m_detailsGrid = FieldGrid();
        s->Add(m_detailsGrid, 0, wxEXPAND);
        m_rawHeader = new SectionHeader(p, "Raw header", true);
        m_rawHeader->SetOpen(false);
        m_headers.push_back(m_rawHeader);
        m_raw = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, FromDIP(180)),
                               wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
        m_raw->SetFont(wxFont(wxFontInfo(GetFont().GetPointSize() - 1).Family(wxFONTFAMILY_TELETYPE)));
        m_raw->Hide();
        m_rawHeader->onToggle = [this](bool open) {
            m_raw->Show(open);
            Relayout();
        };
        s->Add(m_rawHeader, 0, wxTOP, FromDIP(6));
        s->Add(m_raw, 0, wxEXPAND | wxTOP, FromDIP(4));
        p->SetSizer(s);
    }

    // Display
    m_display = AddSection("Display", false);
    {
        wxWindow *p = m_display.body;
        auto s = new wxBoxSizer(wxVERTICAL);
        auto grid = FieldGrid();
        auto choice = [&](const wxString& label, std::initializer_list<wxString> options, wxStaticText **name = nullptr) {
            wxStaticText *t = Secondary(new wxStaticText(p, wxID_ANY, label));
            m_labels.push_back(t);
            grid->Add(t, 0, wxALIGN_CENTER_VERTICAL);
            auto control = new SegmentedControl(p, wxID_ANY, options, label);
            grid->Add(control);
            control->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
                if (onDisplay)
                    onDisplay(Options());
            });
            if (name)
                *name = t;
            return control;
        };
        m_units = choice("Units", { "arc-sec", "pixels" });
        m_axes = choice("Axes", { "RA/Dec", "dx/dy" });
        m_xaxis = choice("Across", { "Frames", "Time" });
        m_device = choice("Pulses", { "Mount", "AO" }, &m_deviceLabel);
        m_axes->SetToolTip("Mount axes (RA and Dec) or the camera's axes (dx and dy)");
        m_xaxis->SetToolTip("Space frames evenly, or by when they were taken");
        m_device->SetToolTip("Whose guide pulses and limits to show");
        s->Add(grid, 0, wxEXPAND);

        auto series = new wxGridSizer(2, wxSize(FromDIP(12), FromDIP(4)));
        auto check = [&](const wxString& label, const wxString& tip) {
            auto cb = new wxCheckBox(p, wxID_ANY, label);
            cb->SetToolTip(tip);
            series->Add(cb);
            cb->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
                if (onDisplay)
                    onDisplay(Options());
            });
            return cb;
        };
        m_showRa = check("RA", "Show the RA (or dx) offset");
        m_showDec = check("Dec", "Show the Dec (or dy) offset");
        m_corrections = check("Guide pulses", "Show each guide pulse as a bar");
        m_events = check("Events", "Show events such as dithers, settling and setting changes");
        m_grid = check("Grid", "Show grid lines and times");
        m_limits = check("Limits", "Show the minimum move and the longest pulse");
        m_mass = check("Star mass", "Show the guide star's mass (brightness)");
        m_snr = check("SNR", "Show the guide star's signal-to-noise ratio");
        s->Add(series, 0, wxEXPAND | wxTOP, FromDIP(10));

        m_lock = new wxCheckBox(p, wxID_ANY, "Lock vertical scale");
        m_lock->SetToolTip("Use the same vertical scale, in arc-seconds, for every session");
        m_lock->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& e) {
            if (onLockVertical)
                onLockVertical(e.IsChecked());
        });
        s->Add(m_lock, 0, wxTOP, FromDIP(10));
        p->SetSizer(s);
    }

    // Exclusions
    m_exclusions = AddSection("Exclusions", false);
    {
        wxWindow *p = m_exclusions.body;
        auto s = new wxBoxSizer(wxVERTICAL);
        m_excluded = Secondary(new wxStaticText(p, wxID_ANY, wxEmptyString));
        s->Add(m_excluded, 0, wxBOTTOM, FromDIP(6));
        m_byServer = new wxCheckBox(p, wxID_ANY, "Settling reported by PHD2");
        m_byDistance = new wxCheckBox(p, wxID_ANY, "Settling by distance");
        m_pauses = new wxCheckBox(p, wxID_ANY, "Paused by server");
        m_byServer->SetToolTip("Exclude frames between PHD2's \"Settling started\" and \"Settling complete\" events");
        m_byDistance->SetToolTip("Exclude frames after each dither until the star stays within the distance set in Preferences");
        m_pauses->SetToolTip("Exclude frames between the server's PAUSE and RESUME commands");
        for (wxCheckBox *cb : { m_byServer, m_byDistance, m_pauses })
        {
            s->Add(cb, 0, wxBOTTOM, FromDIP(4));
            cb->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
                if (onExclusions)
                    onExclusions(Rules());
            });
        }
        auto buttons = new wxBoxSizer(wxHORIZONTAL);
        m_includeAll = new wxButton(p, wxID_ANY, "Include All");
        m_excludeAll = new wxButton(p, wxID_ANY, "Exclude All");
        buttons->Add(m_includeAll, 0, wxRIGHT, FromDIP(6));
        buttons->Add(m_excludeAll);
        m_includeAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { if (onIncludeAll) onIncludeAll(true); });
        m_excludeAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { if (onIncludeAll) onIncludeAll(false); });
        s->Add(buttons, 0, wxTOP, FromDIP(4));
        p->SetSizer(s);
    }

    AlignLabels();
}

Inspector::Section Inspector::AddSection(const wxString& title, bool open)
{
    Section section;
    section.header = new SectionHeader(this, title);
    section.header->SetOpen(open);
    section.body = new wxPanel(this);
    section.body->Show(open);
    m_headers.push_back(section.header);

    wxPanel *body = section.body;
    section.header->onToggle = [this, body](bool isOpen) {
        body->Show(isOpen);
        Relayout();
    };

    int pad = FromDIP(12);
    GetSizer()->Add(section.header, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(8));
    GetSizer()->Add(section.body, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
    return section;
}

wxStaticText *Inspector::AddField(wxWindow *parent, wxFlexGridSizer *grid, const wxString& label)
{
    wxStaticText *name = Secondary(new wxStaticText(parent, wxID_ANY, label));
    m_labels.push_back(name);
    grid->Add(name);
    auto value = new wxStaticText(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    // narrow at minimum, so a long value ends in an ellipsis instead of
    // widening the inspector past its edge
    value->SetMinSize(wxSize(FromDIP(40), -1));
    grid->Add(value, 1, wxEXPAND);
    return value;
}

// Gives every field label the width of the widest, so values line up from
// one section to the next.
void Inspector::AlignLabels()
{
    int widest = 0;
    for (wxStaticText *t : m_labels)
        widest = std::max(widest, t->GetBestSize().x);
    for (wxStaticText *t : m_labels)
        t->SetMinSize(wxSize(widest, -1));
}

// Replaces the fields in a grid.
void Inspector::FillFields(wxWindow *parent, wxFlexGridSizer *grid, const std::vector<DetailField>& fields)
{
    for (wxSizerItem *item : grid->GetChildren())
    {
        wxWindow *w = item->GetWindow();
        std::erase_if(m_secondary, [w](wxStaticText *t) { return t == w; });
        std::erase_if(m_labels, [w](wxStaticText *t) { return t == w; });
    }
    grid->Clear(true);
    for (const DetailField& f : fields)
    {
        wxStaticText *value = AddField(parent, grid, U(f.label));
        value->SetLabel(U(f.value));
        value->SetToolTip(U(f.value));
    }
    AlignLabels();
}

wxStaticText *Inspector::Secondary(wxStaticText *text)
{
    text->SetForegroundColour(m_palette.text2);
    m_secondary.push_back(text);
    return text;
}

void Inspector::SetPalette(const Palette& palette)
{
    m_palette = palette;
    for (wxStaticText *t : m_secondary)
        t->SetForegroundColour(palette.text2);
    for (SectionHeader *h : m_headers)
        h->SetPalette(palette);
    for (SegmentedControl *c : { m_units, m_axes, m_xaxis, m_device, m_driftModel })
        c->SetPalette(palette);
    Refresh();
}

void Inspector::Relayout()
{
    Layout();
    FitInside();
}

DisplayOptions Inspector::Options() const
{
    DisplayOptions d = m_options;
    d.units = m_units->GetSelection() == 0 ? Units::ArcSeconds : Units::Pixels;
    d.raDecAxes = m_axes->GetSelection() == 0;
    d.xaxis = m_xaxis->GetSelection() == 0 ? XAxis::Frames : XAxis::Time;
    d.device = m_device->GetSelection() == 0 ? MOUNT : AO;
    d.ra = m_showRa->GetValue();
    d.dec = m_showDec->GetValue();
    d.corrections = m_corrections->GetValue();
    d.events = m_events->GetValue();
    d.grid = m_grid->GetValue();
    d.limits = m_limits->GetValue();
    d.mass = m_mass->GetValue();
    d.snr = m_snr->GetValue();
    return d;
}

ExclusionSettings Inspector::Rules() const
{
    ExclusionSettings x;
    x.settlingByServer = m_byServer->GetValue();
    x.settlingByDistance = m_byDistance->GetValue();
    x.serverPauses = m_pauses->GetValue();
    return x;
}

void Inspector::ShowDocument(const Document& doc, const AppSettings& settings)
{
    wxWindowUpdateLocker lock(this);

    const GuideSession *session = doc.Session();
    const LogSection *section = session;
    if (!section)
        section = doc.CalibrationSection();

    m_empty->Show(!section);
    const Calibration *cal = doc.CalibrationSection();
    for (Section *s : { &m_summary, &m_drift, &m_display, &m_exclusions })
        s->Show(session != nullptr);
    m_calibration.Show(cal != nullptr);
    bool analysing = session && (settings.view == ChartKind::Drift || settings.view == ChartKind::Frequency);
    m_analysis.Show(analysing);
    if (analysing)
    {
        const auto& sel = doc.Selection();
        size_t begin = sel ? sel->first : 0, end = sel ? sel->last + 1 : session->entries.size();
        wxString scope = sel ? wxString::Format("Frames %u to %u", sel->first + 1, sel->last + 1) : wxString("Whole session");
        if (!GARun::CanAnalyze(*session, begin, end))
            scope += wxString::FromUTF8(" \u00b7 too few frames: analysis needs 12 that count");
        m_scope->SetLabel(scope);
        m_driftModel->SetSelection((int) settings.analysis.drift);
        m_rawRa->SetValue(settings.analysis.rawRa);
        m_selectGa->Show(!GuidingAssistantRuns(*session).empty());
    }
    m_details.Show(section != nullptr);
    if (cal)
        FillFields(m_calibration.body, m_calibrationGrid, CalibrationDetails(*cal));

    if (session)
    {
        Units units = settings.display.units;
        size_t frames = doc.Selection() ? doc.Selection()->last - doc.Selection()->first + 1 : session->entries.size();
        SummaryText t = Summarize(doc.Stats(), frames, session->pixelScale, units);

        m_summary.header->SetTitle(doc.Selection() ? "Selection" : "Summary");
        m_total->SetLabel(U(t.total));
        m_alt->SetLabel(U("total RMS · " + t.alt));
        m_ra->SetLabel(U(t.ra));
        m_dec->SetLabel(U(t.dec));
        m_peak->SetLabel(U(t.peak));
        m_elongation->SetLabel(U(t.elongation));
        m_frames->SetLabel(U(t.frames));
        m_raDrift->SetLabel(U(t.raDrift));
        m_decDrift->SetLabel(U(t.decDrift));
        m_paError->SetLabel(U(t.paError));

        const DisplayOptions& d = settings.display;
        m_options = d;
        m_units->SetSelection(d.units == Units::ArcSeconds ? 0 : 1);
        m_axes->SetSelection(d.raDecAxes ? 0 : 1);
        m_xaxis->SetSelection(d.xaxis == XAxis::Frames ? 0 : 1);
        m_device->SetSelection(d.device == MOUNT ? 0 : 1);
        bool both = HasBothDevices(*session);
        m_device->Show(both);
        m_deviceLabel->Show(both);
        m_showRa->SetValue(d.ra);
        m_showDec->SetValue(d.dec);
        m_corrections->SetValue(d.corrections);
        m_events->SetValue(d.events);
        m_grid->SetValue(d.grid);
        m_limits->SetValue(d.limits);
        m_mass->SetValue(d.mass);
        m_snr->SetValue(d.snr);
        m_showRa->SetLabel(d.raDecAxes ? "RA" : "dx");
        m_showDec->SetLabel(d.raDecAxes ? "Dec" : "dy");
        m_lock->SetValue(doc.VerticalScaleLocked());

        size_t excluded = 0;
        for (const auto& e : session->entries)
            if (!e.included)
                excluded++;
        m_excluded->SetLabel(excluded == 0 ? wxString("No frames excluded") :
                             wxString::Format("%zu of %zu frames excluded", excluded, session->entries.size()));
        m_byServer->SetValue(settings.exclusions.settlingByServer);
        m_byDistance->SetValue(settings.exclusions.settlingByDistance);
        m_pauses->SetValue(settings.exclusions.serverPauses);
    }

    if (section)
    {
        FillFields(m_details.body, m_detailsGrid, SessionDetails(section->hdr));

        wxString raw;
        for (const wxString& line : section->hdr)
            raw << line << "\n";
        m_raw->ChangeValue(raw);
        AlignLabels();
    }

    Relayout();
}
