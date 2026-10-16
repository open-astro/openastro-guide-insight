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

#ifndef SYNTHLOG_INCLUDED
#define SYNTHLOG_INCLUDED

// Writes guide logs in PHD2's format for tests, so a rule can be checked
// against a log with exactly the problem it looks for, read by the real
// parser.

#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace synth {

struct Frame
{
    double t = 0.;
    double ra = 0., dec = 0.;    // raw distances, guide pixels
    int raPulse = 0;             // ms; positive east, negative west
    int decPulse = 0;            // ms; positive north, negative south
    int mass = 400000;
    double snr = 40.;
    int err = 0;
};

// The header lines of a guiding section, from the RedCat 51 log.
inline std::vector<std::string> GuidingHeader()
{
    return {
        "Equipment Profile = RedCat 51",
        "Dither = both axes, Dither scale = 1.000, Image noise reduction = none, Guide-frame time lapse = 0, Server enabled",
        "Pixel scale = 6.45 arc-sec/px, Binning = 1, Focal length = 120 mm",
        "Search region = 15 px, Star mass tolerance = 50.0%, Single-star mode",
        "Camera = Alpaca Camera [localhost:6800/1] - ZWO ASI120MM Mini (ZWO ASI Camera Driver), full size = 1280 x 960, have dark, dark dur = 1000, no defect map, pixel size = 3.8 um",
        "Exposure = 2000 ms",
        "Mount = Alpaca Mount [localhost:6800/0] - iOptron HAE29C EQ, connected, guiding enabled, xAngle = -114.5, xRate = 0.822, yAngle = -23.6, yRate = 1.045, parity = +/+",
        "Norm rates RA = 7.4\"/s @ dec 0, Dec = 6.7\"/s; ortho.err. = 0.9 deg",
        "X guide algorithm = Hysteresis, Hysteresis = 0.100, Aggression = 0.700, Minimum move = 0.150",
        "Y guide algorithm = Resist Switch, Minimum move = 0.150 Aggression = 50% FastSwitch = enabled",
        "Backlash comp = disabled, pulse = 0 ms",
        "Max RA duration = 2500, Max DEC duration = 2500, DEC guide mode = Auto",
        "RA Guide Speed = 7.5 a-s/s, Dec Guide Speed = 7.5 a-s/s, Cal Dec = 44.5, Last Cal Issue = None, Timestamp = Sun 04 Oct 2026 07:06:47 BST",
        "RA = 21.00 hr, Dec = 44.4 deg, Hour angle = 3.07 hr, Pier side = East, Rotator pos = N/A, Alt = 56.0 deg, Az = 293.3 deg",
        "Lock position = 1185.020, 270.028, Star position = 1185.020, 270.028, HFD = 3.66 px",
    };
}

// The header with the line starting `prefix` replaced by `line`, or with
// `line` added when there is none.
inline std::vector<std::string> With(std::vector<std::string> hdr, const std::string& prefix, const std::string& line)
{
    for (std::string& l : hdr)
        if (l.compare(0, prefix.size(), prefix) == 0)
        {
            l = line;
            return hdr;
        }
    hdr.push_back(line);
    return hdr;
}

// A repeatable uniform number in [-1, 1).
struct Noise
{
    unsigned int state = 12345;
    double operator()()
    {
        state = state * 1103515245u + 12345u;
        return ((state >> 8) & 0xffff) / 32768. - 1.;
    }
};

// n frames, one every `cadence` seconds, with random RA and Dec errors of
// the given size and no pulses.
inline std::vector<Frame> Frames(size_t n, double cadence = 2.5, double size = 0.3)
{
    Noise ra, dec;
    dec.state = 777;
    std::vector<Frame> v(n);
    for (size_t i = 0; i < n; i++)
    {
        v[i].t = 2. + i * cadence;
        v[i].ra = size * ra();
        v[i].dec = size * dec();
    }
    return v;
}

class Log
{
    std::ostringstream m_out;

public:
    Log()
    {
        m_out << "PHD2 version 2.6.13, Log version 2.5. Log enabled at 2026-10-04 06:50:00\n\n";
    }

    // A calibration with the given number of steps west, east, north and south.
    Log& Calibration(const std::string& start, int west, int east, int north, int south,
                     const std::string& pierSide = "East")
    {
        m_out << "Calibration Begins at " << start << "\n"
              << "Equipment Profile = RedCat 51\n"
              << "Pixel scale = 6.45 arc-sec/px, Binning = 1, Focal length = 120 mm\n"
              << "Mount = Alpaca Mount [localhost:6800/0] - iOptron HAE29C EQ, Calibration Step = 750 ms, "
                 "Calibration Distance = 25 px, Assume orthogonal axes = no\n"
              << "RA = 21.00 hr, Dec = 44.4 deg, Hour angle = 2.85 hr, Pier side = " << pierSide << "\n"
              << "Direction,Step,dx,dy,x,y,Dist\n";
        auto steps = [this](const char *dir, int n, double sx, double sy) {
            for (int i = 0; i < n; i++)
                m_out << dir << "," << i << "," << sx * i << "," << sy * i << ",0,0," << 2. * i << "\n";
        };
        steps("West", west, -1.5, -1.2);
        steps("East", east, 1.5, 1.2);
        steps("North", north, 1.0, -1.4);
        steps("South", south, -1.0, 1.4);
        m_out << "West calibration complete. Angle = -114.5 deg, Rate = 0.822 px/sec, Parity = Even\n"
              << "North calibration complete. Angle = -23.6 deg, Rate = 1.045 px/sec, Parity = Even\n"
              << "Calibration complete, mount = Alpaca Mount.\n\n";
        return *this;
    }

    // A guiding section; `infos` puts INFO lines before the frame with that index.
    Log& Guiding(const std::string& start, const std::vector<std::string>& hdr, const std::vector<Frame>& frames,
                 const std::multimap<size_t, std::string>& infos = {})
    {
        m_out << "Guiding Begins at " << start << "\n";
        for (const std::string& l : hdr)
            m_out << l << "\n";
        m_out << "Frame,Time,mount,dx,dy,RARawDistance,DECRawDistance,RAGuideDistance,DECGuideDistance,RADuration,"
                 "RADirection,DECDuration,DECDirection,XStep,YStep,StarMass,SNR,ErrorCode\n";
        for (size_t i = 0; i < frames.size(); i++)
        {
            auto range = infos.equal_range(i);
            for (auto it = range.first; it != range.second; ++it)
                m_out << "INFO: " << it->second << "\n";
            const Frame& f = frames[i];
            m_out << i + 1 << "," << f.t << ",\"Mount\"," << f.ra << "," << f.dec << "," << f.ra << "," << f.dec
                  << ",0,0,";
            if (f.raPulse)
                m_out << (f.raPulse > 0 ? f.raPulse : -f.raPulse) << "," << (f.raPulse > 0 ? "E" : "W");
            else
                m_out << "0,";
            m_out << ",";
            if (f.decPulse)
                m_out << (f.decPulse > 0 ? f.decPulse : -f.decPulse) << "," << (f.decPulse > 0 ? "N" : "S");
            else
                m_out << "0,";
            m_out << ",,," << f.mass << "," << f.snr << "," << f.err << "\n";
        }
        m_out << "Guiding Ends at " << start << "\n\n";
        return *this;
    }

    std::string Text() const { return m_out.str(); }
};

} // namespace synth

#endif
