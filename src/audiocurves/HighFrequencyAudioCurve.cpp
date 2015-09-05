/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2014 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/

#include "HighFrequencyAudioCurve.h"
#include <numeric>
#include <algorithm>
#include <utility>
namespace Rubbers
{


HighFrequencyAudioCurve::HighFrequencyAudioCurve(Parameters parameters) :
    AudioCurveCalculator(parameters)
{}

HighFrequencyAudioCurve::~HighFrequencyAudioCurve()
{}

void
HighFrequencyAudioCurve::reset()
{}

float
HighFrequencyAudioCurve::process(const float *R__ mag, int increment){
    auto n = 0;
    return std::accumulate(mag, mag+m_lastPerceivedBin,0.f,[&n](const auto &a, const auto &b){auto c = a + n*b;n++;return c;});

}

double
HighFrequencyAudioCurve::process(const double *R__ mag, int increment){
    auto n = 0;
    return std::accumulate(mag, mag+m_lastPerceivedBin,0.0,[&n](const auto &a, const auto &b){auto c = a + n*b;n++;return c;});
}

}

