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

#include "PercussiveAudioCurve.h"

#include "system/Allocators.h"
#include "system/VectorOps.h"

#include <cmath>
#include <iostream>
namespace RubberBand
{


PercussiveAudioCurve::PercussiveAudioCurve(Parameters parameters) :
    AudioCurveCalculator(parameters)
{m_prevMag = std::make_unique<float[]>(m_fftSize/2+1);}
PercussiveAudioCurve::~PercussiveAudioCurve(){}

void
PercussiveAudioCurve::reset(){v_zero(&m_prevMag[0], m_fftSize/2 + 1);}
void
PercussiveAudioCurve::setFftSize(int newSize){
    m_prevMag = std::make_unique<float[]>(newSize/2 + 1);
    AudioCurveCalculator::setFftSize(newSize);
    reset();
}
float
PercussiveAudioCurve::process(const float *R__ mag, int increment){
    static const auto threshold = static_cast<float>(std::pow(10.f, 0.15f)); // 3dB rise in square of magnitude
    static const auto zeroThresh = static_cast<float>(pow(10.f, -8.f));
    auto count = 0;
    auto nonZeroCount = 0;
    const auto sz = m_lastPerceivedBin;
    for ( auto n = 1; n <= sz; ++n ){
        count += ( ( m_prevMag[n] > zeroThresh ) && ( mag[n] / m_prevMag[n] > threshold ) ) || ( mag[n] > zeroThresh );
        nonZeroCount += mag[n] > zeroThresh;
        m_prevMag[n] = mag[n];
    }
    return nonZeroCount ? (static_cast<float>(count)/nonZeroCount) : 0;
    if (nonZeroCount == 0) return 0;
    else return float(count) / float(nonZeroCount);
}
double
PercussiveAudioCurve::process(const double *R__ mag, int increment){
    static const auto threshold = std::pow(10., 0.15); // 3dB rise in square of magnitude
    static const auto  zeroThresh = std::pow(10., -8);
    auto count = 0;
    auto nonZeroCount = 0;
    const auto sz = m_lastPerceivedBin;
    for ( auto n = 1; n <= sz; ++n ){
        count += ( ( m_prevMag[n] > zeroThresh ) && ( mag[n] / m_prevMag[n] > threshold ) ) || (mag[n] > zeroThresh);
        nonZeroCount += mag[n] > zeroThresh;
        m_prevMag[n] = mag[n];
    }
    return nonZeroCount ? ( static_cast<double>(count) / nonZeroCount ) : 0;
}


}

