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

#include "icons.h"

#include <string>

// 24x24 line icons; COLOR is replaced by the stroke colour.
static const char *Svg(Icon icon)
{
    switch (icon)
    {
    case Icon::Open:
        return "<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 24'>"
               "<path d='M3.5 7V17.5A1.5 1.5 0 0 0 5 19H19A1.5 1.5 0 0 0 20.5 17.5V9.5A1.5 1.5 0 0 0 19 8H11.8L9.8 5.5H5A1.5 1.5 0 0 0 3.5 7Z' "
               "fill='none' stroke='COLOR' stroke-width='1.6' stroke-linejoin='round'/></svg>";
    case Icon::Sidebar:
        return "<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 24'>"
               "<rect x='3.5' y='5' width='17' height='14' rx='2.5' fill='none' stroke='COLOR' stroke-width='1.6'/>"
               "<path d='M9.5 5V19' stroke='COLOR' stroke-width='1.6'/>"
               "<path d='M5.8 8.5H7.6M5.8 11H7.6M5.8 13.5H7.6' stroke='COLOR' stroke-width='1.3' stroke-linecap='round'/></svg>";
    case Icon::Inspector:
        return "<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 24'>"
               "<rect x='3.5' y='5' width='17' height='14' rx='2.5' fill='none' stroke='COLOR' stroke-width='1.6'/>"
               "<path d='M14.5 5V19' stroke='COLOR' stroke-width='1.6'/>"
               "<path d='M16.6 8.5H18.2M16.6 11H18.2M16.6 13.5H18.2' stroke='COLOR' stroke-width='1.3' stroke-linecap='round'/></svg>";
    }
    return "";
}

wxBitmapBundle ToolIcon(Icon icon, const wxColour& color)
{
    std::string svg = Svg(icon);
    std::string hex = color.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    for (size_t pos; (pos = svg.find("COLOR")) != std::string::npos;)
        svg.replace(pos, 5, hex);
    return wxBitmapBundle::FromSVG(svg.c_str(), wxSize(20, 20));
}
