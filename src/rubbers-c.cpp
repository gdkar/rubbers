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

#include "rubbers/rubbers-c.h"
#include "rubbers/RubbersStretcher.h"

struct RubbersState_
{
    Rubbers::RubbersStretcher *m_s;
};

RubbersState rubbers_new(unsigned int sampleRate,
                               unsigned int channels,
                               RubbersOptions options,
                               double initialTimeRatio,
                               double initialPitchScale)
{
    RubbersState_ *state = new RubbersState_();
    state->m_s = new Rubbers::RubbersStretcher
        (sampleRate, channels, options,
         initialTimeRatio, initialPitchScale);
    return state;
}

void rubbers_delete(RubbersState state)
{
    delete state->m_s;
    delete state;
}

void rubbers_reset(RubbersState state)
{
    state->m_s->reset();
}

void rubbers_set_time_ratio(RubbersState state, double ratio)
{
    state->m_s->setTimeRatio(ratio);
}

void rubbers_set_pitch_scale(RubbersState state, double scale)
{
    state->m_s->setPitchScale(scale);
}

double rubbers_get_time_ratio(const RubbersState state) 
{
    return state->m_s->getTimeRatio();
}

double rubbers_get_pitch_scale(const RubbersState state)
{
    return state->m_s->getPitchScale();
}

size_t rubbers_get_latency(const RubbersState state) 
{
    return state->m_s->getLatency();
}

void rubbers_set_transients_option(RubbersState state, RubbersOptions options)
{
    state->m_s->setTransientsOption(options);
}

void rubbers_set_detector_option(RubbersState state, RubbersOptions options)
{
    state->m_s->setDetectorOption(options);
}

void rubbers_set_phase_option(RubbersState state, RubbersOptions options)
{
    state->m_s->setPhaseOption(options);
}

void rubbers_set_formant_option(RubbersState state, RubbersOptions options)
{
    state->m_s->setFormantOption(options);
}

void rubbers_set_pitch_option(RubbersState state, RubbersOptions options)
{
    state->m_s->setPitchOption(options);
}

void rubbers_set_expected_input_duration(RubbersState state, size_t samples)
{
    state->m_s->setExpectedInputDuration(samples);
}

size_t rubbers_get_samples_required(const RubbersState state)
{
    return state->m_s->getSamplesRequired();
}

void rubbers_set_max_process_size(RubbersState state, size_t samples)
{
    state->m_s->setMaxProcessSize(samples);
}

void rubbers_set_key_frame_map(RubbersState state, size_t keyframecount, size_t *from, size_t  *to)
{
    std::map<size_t, size_t> kfm;
    for (size_t i = 0; i < keyframecount; ++i) {
        kfm[from[i]] = to[i];
    }
    state->m_s->setKeyFrameMap(kfm);
}

void rubbers_study(RubbersState state, const float *const *input, size_t samples, bool flush)
{
    state->m_s->study(input, samples, flush != 0);
}

void rubbers_process(RubbersState state, const float *const *input, size_t samples, bool flush)
{
    state->m_s->process(input, samples, flush!= 0);
}

ssize_t rubbers_available(const RubbersState state)
{
    return state->m_s->available();
}

size_t rubbers_retrieve(const RubbersState state, float *const *output, size_t samples)
{
    return state->m_s->retrieve(output, samples);
}

unsigned int rubbers_get_channel_count(const RubbersState state)
{
    return state->m_s->getChannelCount();
}

void rubbers_calculate_stretch(RubbersState state)
{
    state->m_s->calculateStretch();
}

void rubbers_set_debug_level(RubbersState state, int level)
{
    state->m_s->setDebugLevel(level);
}

void rubbers_set_default_debug_level(int level)
{
    Rubbers::RubbersStretcher::setDefaultDebugLevel(level);
}

