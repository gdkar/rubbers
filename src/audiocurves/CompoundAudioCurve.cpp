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

#include "CompoundAudioCurve.h"
#include "dsp/MovingMedian.h"
#include <iostream>
namespace RubberBand
{
CompoundAudioCurve::CompoundAudioCurve(Parameters parameters) :
    AudioCurveCalculator(parameters),
    m_percussive(parameters),
    m_hf(parameters),
    m_hfFilter(new MovingMedian<float>(19, 85)),
    m_hfDerivFilter(new MovingMedian<float>(19, 90)),
    m_type(CompoundDetector),
    m_lastHf(0.0),
    m_lastResult(0.0),
    m_risingCount(0)
{}
CompoundAudioCurve::~CompoundAudioCurve(){}
void
CompoundAudioCurve::setType(Type type){m_type = type;}
void
CompoundAudioCurve::reset(){
    m_percussive.reset();
    m_hf.reset();
    m_hfFilter->reset();
    m_hfDerivFilter->reset();
    m_lastHf = 0.0;
    m_lastResult = 0.0;
}
void
CompoundAudioCurve::setFftSize(int newSize){
    m_percussive.setFftSize(newSize);
    m_hf.setFftSize(newSize);
    m_fftSize = newSize;
    m_lastHf = 0.0;
    m_lastResult = 0.0;
}
float
CompoundAudioCurve::process(const float *R__ mag, int increment){
    float percussive = 0.f;
    float hf = 0.f;
    switch (m_type) {
    case PercussiveDetector:
        percussive = m_percussive.process(mag, increment);
        break;
    case CompoundDetector:
        percussive = m_percussive.process(mag, increment);
        hf = m_hf.process(mag, increment);
        break;
    case SoftDetector:
        hf = m_hf.process(mag, increment);
        break;
    }
    return processFiltering(percussive, hf);
}
double
CompoundAudioCurve::process(const double *R__ mag, int increment){
    double percussive = 0.0;
    double hf = 0.0;
    switch (m_type) {
    case PercussiveDetector:
        percussive = m_percussive.process(mag, increment);
        break;
    case CompoundDetector:
        percussive = m_percussive.process(mag, increment);
        hf = m_hf.process(mag, increment);
        break;
    case SoftDetector:
        hf = m_hf.process(mag, increment);
        break;
    }
    return processFiltering(percussive, hf);
}
float 
CompoundAudioCurve::processFiltering(float percussive, float hf){
    if (m_type == PercussiveDetector) {return percussive;}
    float rv = 0.f;
    float hfDeriv = hf - m_lastHf;
    m_hfFilter->push(hf);
    m_hfDerivFilter->push(hfDeriv);
    float hfFiltered = m_hfFilter->get();
    float hfDerivFiltered = m_hfDerivFilter->get();
    m_lastHf = hf;
    float result = 0.f;
    float hfExcess = hf - hfFiltered;
    if (hfExcess > 0.0) {result = hfDeriv - hfDerivFiltered;}
    if (result < m_lastResult) {
        if (m_risingCount > 3 && m_lastResult > 0) rv = 0.5;
        m_risingCount = 0;
    }
    else {m_risingCount ++;}
    if (m_type == CompoundDetector) {
        if (percussive > 0.35 && percussive > rv) {rv = percussive;}
    }
    m_lastResult = result;
    return rv;
}


}

