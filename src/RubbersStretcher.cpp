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

#include "StretcherImpl.h"
using namespace std;
namespace Rubbers {
RubbersStretcher::RubbersStretcher(size_t sampleRate,
                                         size_t channels,
                                         Options options,
                                         double initialTimeRatio,
                                         double initialPitchScale) :
    m_d(new Impl(sampleRate, channels, options,initialTimeRatio, initialPitchScale))
{}
RubbersStretcher::~RubbersStretcher(){delete m_d;}
void
RubbersStretcher::reset(){m_d->reset();}
void
RubbersStretcher::setTimeRatio(double ratio){m_d->setTimeRatio(ratio);}
void
RubbersStretcher::setPitchScale(double scale){m_d->setPitchScale(scale);}
double
RubbersStretcher::getTimeRatio() const{return m_d->getTimeRatio();}
double
RubbersStretcher::getPitchScale() const{return m_d->getPitchScale();}
size_t
RubbersStretcher::getLatency() const{return m_d->getLatency();}
void
RubbersStretcher::setTransientsOption(Options options) {m_d->setTransientsOption(options);}
void
RubbersStretcher::setDetectorOption(Options options) {m_d->setDetectorOption(options);}
void
RubbersStretcher::setPhaseOption(Options options) {m_d->setPhaseOption(options);}
void
RubbersStretcher::setFormantOption(Options options){m_d->setFormantOption(options);}
void
RubbersStretcher::setPitchOption(Options options){m_d->setPitchOption(options);}
void
RubbersStretcher::setExpectedInputDuration(size_t samples) {m_d->setExpectedInputDuration(samples);}
void
RubbersStretcher::setMaxProcessSize(size_t samples){m_d->setMaxProcessSize(samples);}
void
RubbersStretcher::setKeyFrameMap(const map<size_t, size_t> &mapping){m_d->setKeyFrameMap(mapping);}
size_t
RubbersStretcher::getSamplesRequired() const{return m_d->getSamplesRequired();}
void
RubbersStretcher::study(const float *const *input, size_t samples,bool done){m_d->study(input, samples, done);}
void
RubbersStretcher::process(const float *const *input, size_t samples,bool done){m_d->process(input, samples, done);}
ssize_t
RubbersStretcher::available() const{return m_d->available();}
size_t
RubbersStretcher::retrieve(float *const *output, size_t samples) const{return m_d->retrieve(output, samples);}
float
RubbersStretcher::getFrequencyCutoff(int n) const{return m_d->getFrequencyCutoff(n);}
void
RubbersStretcher::setFrequencyCutoff(int n, float f) {m_d->setFrequencyCutoff(n, f);}
size_t
RubbersStretcher::getInputIncrement() const{return m_d->getInputIncrement();}
vector<int>
RubbersStretcher::getOutputIncrements() const{return m_d->getOutputIncrements();}
vector<float>
RubbersStretcher::getPhaseResetCurve() const{return m_d->getPhaseResetCurve();}
vector<int>
RubbersStretcher::getExactTimePoints() const{return m_d->getExactTimePoints();}
size_t
RubbersStretcher::getChannelCount() const{return m_d->getChannelCount();}
void
RubbersStretcher::calculateStretch(){m_d->calculateStretch();}
void
RubbersStretcher::setDebugLevel(int level){m_d->setDebugLevel(level);}
void
RubbersStretcher::setDefaultDebugLevel(int level){Impl::setDefaultDebugLevel(level);}

}

