/*
 *  This file is part of phdlogview
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

#include "settingsmigration.h"

#include <wx/arrstr.h>

static bool IsEmpty(wxConfigBase& c)
{
    return c.GetNumberOfEntries(true) == 0 && c.GetNumberOfGroups(true) == 0;
}

// Copies the entries of `from`'s current group, then recurses into its
// subgroups. Both configs' paths are restored before returning.
static void CopyGroup(wxConfigBase& from, wxConfigBase& to)
{
    const wxString fromPath = from.GetPath();
    const wxString toPath = to.GetPath();

    wxString name;
    long cookie;
    for (bool more = from.GetFirstEntry(name, cookie); more; more = from.GetNextEntry(name, cookie))
        to.Write(name, from.Read(name, wxEmptyString));

    wxArrayString groups;
    for (bool more = from.GetFirstGroup(name, cookie); more; more = from.GetNextGroup(name, cookie))
        groups.Add(name);

    for (const wxString& group : groups)
    {
        from.SetPath(group);
        to.SetPath(group);
        CopyGroup(from, to);
        from.SetPath(fromPath);
        to.SetPath(toPath);
    }
}

bool MigrateSettings(wxConfigBase& from, wxConfigBase& to)
{
    if (!IsEmpty(to) || IsEmpty(from))
        return false;

    const wxString fromPath = from.GetPath();
    const wxString toPath = to.GetPath();
    from.SetPath("/");
    to.SetPath("/");
    CopyGroup(from, to);
    to.Flush();
    from.SetPath(fromPath);
    to.SetPath(toPath);
    return true;
}
