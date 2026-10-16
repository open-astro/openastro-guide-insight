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

#include "Preferences.h"

#include "theme.h"

#include <wx/button.h>
#include <wx/clrpicker.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

namespace {

class GeneralPanel : public wxPanel
{
    AppSettings& m_settings;
    std::function<void()> m_changed;
    wxColourPickerCtrl *m_ra, *m_dec;
    wxSpinCtrlDouble *m_pixels, *m_seconds;

    void ShowColours()
    {
        Palette theme = CurrentPalette(m_settings.raColor, m_settings.decColor);
        m_ra->SetColour(theme.ra);
        m_dec->SetColour(theme.dec);
    }

    void Apply()
    {
        Palette theme = CurrentPalette();
        auto chosen = [](const wxColour& c, const wxColour& themed) {
            return c == themed ? std::string() : c.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
        };
        m_settings.raColor = chosen(m_ra->GetColour(), theme.ra);
        m_settings.decColor = chosen(m_dec->GetColour(), theme.dec);
        m_settings.exclusions.settle.pixels = m_pixels->GetValue();
        m_settings.exclusions.settle.seconds = m_seconds->GetValue();
        if (m_changed)
            m_changed();
    }

    void Changed()
    {
        if (wxPreferencesEditor::ShouldApplyChangesImmediately())
            Apply();
    }

public:
    GeneralPanel(wxWindow *parent, AppSettings& settings, std::function<void()> changed)
        : wxPanel(parent), m_settings(settings), m_changed(std::move(changed))
    {
        auto sizer = new wxBoxSizer(wxVERTICAL);
        int gap = FromDIP(8);

        auto heading = [this](const wxString& text) {
            auto t = new wxStaticText(this, wxID_ANY, text);
            t->SetFont(GetFont().Bold());
            return t;
        };

        sizer->Add(heading("Series colours"), 0, wxBOTTOM, gap);
        auto colours = new wxFlexGridSizer(3, wxSize(gap, gap));
        m_ra = new wxColourPickerCtrl(this, wxID_ANY);
        m_dec = new wxColourPickerCtrl(this, wxID_ANY);
        colours->Add(new wxStaticText(this, wxID_ANY, "RA"), 0, wxALIGN_CENTER_VERTICAL);
        colours->Add(m_ra);
        auto reset = new wxButton(this, wxID_ANY, "Use Theme Colours");
        colours->Add(reset, 0, wxLEFT, gap);
        colours->Add(new wxStaticText(this, wxID_ANY, "Dec"), 0, wxALIGN_CENTER_VERTICAL);
        colours->Add(m_dec);
        sizer->Add(colours, 0, wxLEFT, gap);

        sizer->AddSpacer(2 * gap);
        sizer->Add(heading("Settling by distance"), 0, wxBOTTOM, gap);
        auto settle = new wxBoxSizer(wxHORIZONTAL);
        m_pixels = new wxSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 0.1, 20., settings.exclusions.settle.pixels, 0.1);
        m_pixels->SetDigits(2);
        m_seconds = new wxSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                         wxSP_ARROW_KEYS, 0., 600., settings.exclusions.settle.seconds, 1.);
        m_seconds->SetDigits(0);
        settle->Add(new wxStaticText(this, wxID_ANY, "Settled when within"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
        settle->Add(m_pixels, 0, wxRIGHT, gap);
        settle->Add(new wxStaticText(this, wxID_ANY, "px for"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
        settle->Add(m_seconds, 0, wxRIGHT, gap);
        settle->Add(new wxStaticText(this, wxID_ANY, "s after a dither"), 0, wxALIGN_CENTER_VERTICAL);
        sizer->Add(settle, 0, wxLEFT, gap);
        auto note = new wxStaticText(this, wxID_ANY, "Turn the rule on in the inspector's Exclusions section.");
        note->SetFont(GetFont().Smaller());
        note->SetForegroundColour(CurrentPalette().text2);
        sizer->Add(note, 0, wxLEFT | wxTOP, gap);

        auto outer = new wxBoxSizer(wxVERTICAL);
        outer->Add(sizer, 1, wxEXPAND | wxALL, 2 * gap);
        SetSizerAndFit(outer);

        ShowColours();

        m_ra->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent&) { Changed(); });
        m_dec->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent&) { Changed(); });
        m_pixels->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) { Changed(); });
        m_seconds->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) { Changed(); });
        reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Palette theme = CurrentPalette();
            m_ra->SetColour(theme.ra);
            m_dec->SetColour(theme.dec);
            Changed();
        });
    }

    bool TransferDataFromWindow() override
    {
        if (!wxPreferencesEditor::ShouldApplyChangesImmediately())
            Apply();
        return true;
    }
};

class GeneralPage : public wxStockPreferencesPage
{
    AppSettings& m_settings;
    std::function<void()> m_changed;

public:
    GeneralPage(AppSettings& settings, std::function<void()> changed)
        : wxStockPreferencesPage(Kind_General), m_settings(settings), m_changed(std::move(changed)) { }

    wxWindow *CreateWindow(wxWindow *parent) override
    {
        return new GeneralPanel(parent, m_settings, m_changed);
    }
};

} // namespace

std::unique_ptr<wxPreferencesEditor> CreatePreferences(AppSettings& settings, std::function<void()> changed)
{
    auto editor = std::make_unique<wxPreferencesEditor>();
    editor->AddPage(new GeneralPage(settings, std::move(changed)));
    return editor;
}
