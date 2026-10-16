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

#include "sessiondetails.h"

#include "labels.h"

#include <charconv>
#include <math.h>
#include <optional>
#include <map>

namespace {

bool StartsWith(const std::string& s, const std::string& prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Where `sep` first occurs in s outside brackets and parentheses, or npos.
size_t FindOutsideBrackets(const std::string& s, const std::string& sep, size_t from = 0)
{
    int depth = 0;
    for (size_t i = from; i < s.size(); i++)
    {
        char c = s[i];
        if (c == '[' || c == '(')
            depth++;
        else if ((c == ']' || c == ')') && depth > 0)
            depth--;
        else if (depth == 0 && s.compare(i, sep.size(), sep) == 0)
            return i;
    }
    return std::string::npos;
}

// The first comma-separated item of a header value.
std::string FirstItem(const std::string& s)
{
    return s.substr(0, FindOutsideBrackets(s, ", "));
}

// The value of "key = value" anywhere in the line, up to the next comma.
std::string Setting(const std::string& line, const std::string& key)
{
    std::string k = key + " = ";
    size_t pos = line.find(k);
    if (pos == std::string::npos)
        return std::string();
    return FirstItem(line.substr(pos + k.size()));
}

std::string StripSuffix(const std::string& s, const std::string& suffix)
{
    return EndsWith(s, suffix) ? s.substr(0, s.size() - suffix.size()) : s;
}

// A camera or mount name. Alpaca devices are logged as
// "Alpaca Camera [host:port/n] - Model (Driver)"; those show the model.
std::string DeviceName(const std::string& value)
{
    std::string name = FirstItem(value);
    size_t dash = FindOutsideBrackets(name, " - ");
    if (dash == std::string::npos)
        return name;

    name = name.substr(dash + 3);
    if (EndsWith(name, ")"))
    {
        size_t open = name.rfind(" (");
        if (open != std::string::npos)
            name = name.substr(0, open);
    }
    return name;
}

std::string Seconds(const std::string& value)
{
    std::string ms = StripSuffix(FirstItem(value), " ms");
    int n = 0;
    auto res = std::from_chars(ms.data(), ms.data() + ms.size(), n);
    if (res.ec != std::errc() || res.ptr != ms.data() + ms.size())
        return FirstItem(value);

    char buf[32];
    double secs = n / 1000.;
    auto out = n % 100 == 0 ?
        std::to_chars(buf, buf + sizeof(buf), secs, std::chars_format::fixed, 1) :
        std::to_chars(buf, buf + sizeof(buf), secs);
    return std::string(buf, out.ptr) + " s";
}

std::string PixelScale(const std::string& line, const std::string& value)
{
    std::string s = StripSuffix(FirstItem(value), " arc-sec/px") + "″/px";
    std::string binning = Setting(line, "Binning");
    if (!binning.empty() && binning != "1")
        s += " · bin " + binning;
    std::string focal = Setting(line, "Focal length");
    if (!focal.empty())
        s += " · " + focal;
    return s;
}

std::string Pointing(const std::string& line)
{
    std::string s;
    std::string dec = Setting(line, "Dec");
    if (!dec.empty())
        s = "Dec " + StripSuffix(dec, " deg") + "°";
    std::string pier = Setting(line, "Pier side");
    if (!pier.empty() && pier != "Unknown" && pier != "N/A")
        s += (s.empty() ? "pier " : " · pier ") + pier;
    return s;
}

} // namespace

std::vector<DetailField> SessionDetails(const wxArrayString& hdr)
{
    std::map<std::string, std::string> found;
    auto add = [&found](const std::string& label, const std::string& value) {
        if (!value.empty())
            found.emplace(label, value); // the first mention wins
    };

    bool ao = false; // the last device line was the AO's
    for (const wxString& wxln : hdr)
    {
        std::string line(wxln.utf8_str());
        auto value = [&line](const std::string& key) { return line.substr(key.size()); };

        if (StartsWith(line, "Equipment Profile = "))
            add("Profile", value("Equipment Profile = "));
        else if (StartsWith(line, "Camera = "))
            add("Camera", DeviceName(value("Camera = ")));
        else if (StartsWith(line, "Mount = "))
        {
            ao = false;
            add("Mount", DeviceName(value("Mount = ")));
        }
        else if (StartsWith(line, "AO = "))
        {
            ao = true;
            add("AO", DeviceName(value("AO = ")));
        }
        else if (StartsWith(line, "Pixel scale = "))
            add("Pixel scale", PixelScale(line, value("Pixel scale = ")));
        else if (StartsWith(line, "Exposure = "))
            add("Exposure", Seconds(value("Exposure = ")));
        else if (StartsWith(line, "X guide algorithm = "))
            add(ao ? "AO X algorithm" : "RA algorithm", FirstItem(value("X guide algorithm = ")));
        else if (StartsWith(line, "Y guide algorithm = "))
            add(ao ? "AO Y algorithm" : "Dec algorithm", FirstItem(value("Y guide algorithm = ")));
        else if (StartsWith(line, "RA = "))
            add("Pointing", Pointing(line));
    }

    static const char *const order[] = {
        "Profile", "Camera", "Mount", "AO", "Pixel scale", "Exposure",
        "RA algorithm", "Dec algorithm", "AO X algorithm", "AO Y algorithm", "Pointing",
    };

    std::vector<DetailField> fields;
    for (const char *label : order)
    {
        auto it = found.find(label);
        if (it != found.end())
            fields.push_back(DetailField{ it->first, it->second });
    }
    return fields;
}

namespace {

std::optional<double> Number(const std::string& text)
{
    double v;
    auto res = std::from_chars(text.data(), text.data() + text.size(), v);
    if (res.ec != std::errc())
        return std::nullopt;
    return v;
}

struct AxisResult
{
    std::string rate;  // pixels per second, as logged
    std::string angle; // degrees, as logged
};

// "West calibration complete. Angle = -114.5 deg, Rate = 0.822 px/sec, Parity = Even"
AxisResult AxisFrom(const std::string& line)
{
    return AxisResult{ StripSuffix(Setting(line, "Rate"), " px/sec"), StripSuffix(Setting(line, "Angle"), " deg") };
}

} // namespace

std::vector<DetailField> CalibrationDetails(const Calibration& cal)
{
    std::optional<double> pixelScale;
    std::optional<AxisResult> ra, dec;
    std::string step;

    for (const wxString& wxln : cal.hdr)
    {
        std::string line(wxln.utf8_str());
        if (StartsWith(line, "Pixel scale = "))
            pixelScale = Number(FirstItem(line.substr(14)));
        else if (StartsWith(line, "West calibration complete."))
            ra = AxisFrom(line);
        else if (StartsWith(line, "North calibration complete."))
            dec = AxisFrom(line);
        else if ((StartsWith(line, "Mount = ") || StartsWith(line, "AO = ")) &&
                 line.find("Calibration Step = ") != std::string::npos)
        {
            std::string distance = Setting(line, "Calibration Distance");
            step = Setting(line, "Calibration Step") + (distance.empty() ? "" : ", " + distance + " target");
        }
    }

    std::vector<DetailField> fields;
    auto rate = [&](const char *label, const AxisResult& axis) {
        std::string text = axis.rate + " px/s";
        std::optional<double> r = Number(axis.rate);
        if (r && pixelScale)
            text += " \u00b7 " + Fixed(*r * *pixelScale, 2) + "\u2033/s";
        fields.push_back(DetailField{ label, text });
    };
    auto angle = [](const std::string& degrees) {
        std::optional<double> a = Number(degrees);
        return a ? Fixed(*a, 1) + "\u00b0" : degrees;
    };

    if (ra)
        rate("RA rate", *ra);
    if (dec)
        rate("Dec rate", *dec);
    if (ra)
        fields.push_back(DetailField{ "RA angle", angle(ra->angle) });
    if (dec)
        fields.push_back(DetailField{ "Dec angle", angle(dec->angle) });
    if (ra && dec)
    {
        std::optional<double> a = Number(ra->angle), b = Number(dec->angle);
        if (a && b)
        {
            double between = fabs(remainder(*a - *b, 360.));
            fields.push_back(DetailField{ "Orthogonality error", Fixed(fabs(between - 90.), 1) + "\u00b0" });
        }
    }
    if (!step.empty())
        fields.push_back(DetailField{ "Step size", step });

    int counts[5] = {};
    for (const auto& e : cal.entries)
        counts[e.direction]++;
    std::string steps;
    // in the order a calibration goes: west, east, backlash clearing, north, south
    for (CalDirection d : { WEST, EAST, BACKLASH, NORTH, SOUTH })
        if (counts[d])
            steps += (steps.empty() ? "" : " \u00b7 ") + std::string(1, "WEBNS"[d]) + " " + std::to_string(counts[d]);
    if (!steps.empty())
        fields.push_back(DetailField{ "Steps", steps });
    return fields;
}
