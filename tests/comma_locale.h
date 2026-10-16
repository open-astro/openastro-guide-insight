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

#ifndef COMMA_LOCALE_INCLUDED
#define COMMA_LOCALE_INCLUDED

#include <clocale>
#include <string>

namespace {

// Switches LC_NUMERIC to a locale whose decimal separator is a comma for the
// lifetime of the object, then restores the previous locale.
class CommaDecimalLocale
{
    std::string m_saved;
    bool m_active = false;

public:
    CommaDecimalLocale()
    {
        m_saved = std::setlocale(LC_NUMERIC, nullptr);
        for (const char *name : { "de_DE.UTF-8", "de_DE.utf8", "de_DE", "de-DE", "German_Germany.1252" })
        {
            if (std::setlocale(LC_NUMERIC, name))
            {
                m_active = *std::localeconv()->decimal_point == ',';
                if (m_active)
                    break;
            }
        }
    }
    ~CommaDecimalLocale() { std::setlocale(LC_NUMERIC, m_saved.c_str()); }
    bool Active() const { return m_active; }
};

} // namespace

#endif
