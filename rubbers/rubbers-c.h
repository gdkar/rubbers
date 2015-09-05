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

#ifndef _rubbers_C_API_H_
#define _rubbers_C_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <sys/types.h>

#define rubbers_VERSION "1.8.1"
#define rubbers_API_MAJOR_VERSION 2
#define rubbers_API_MINOR_VERSION 5

/**
 * This is a C-linkage interface to the Rubber Band time stretcher.
 * 
 * This is a wrapper interface: the primary interface is in C++ and is
 * defined and documented in RubbersStretcher.h.  The library
 * itself is implemented in C++, and requires C++ standard library
 * support even when using the C-linkage API.
 *
 * Please see RubbersStretcher.h for documentation.
 *
 * If you are writing to the C++ API, do not include this header.
 */

enum RubbersOption {

    RubbersOptionProcessOffline       = 0x00000000,
    RubbersOptionProcessRealTime      = 0x00000001,

    RubbersOptionStretchElastic       = 0x00000000,
    RubbersOptionStretchPrecise       = 0x00000010,
    
    RubbersOptionTransientsCrisp      = 0x00000000,
    RubbersOptionTransientsMixed      = 0x00000100,
    RubbersOptionTransientsSmooth     = 0x00000200,

    RubbersOptionDetectorCompound     = 0x00000000,
    RubbersOptionDetectorPercussive   = 0x00000400,
    RubbersOptionDetectorSoft         = 0x00000800,

    RubbersOptionPhaseLaminar         = 0x00000000,
    RubbersOptionPhaseIndependent     = 0x00002000,
    
    RubbersOptionThreadingAuto        = 0x00000000,
    RubbersOptionThreadingNever       = 0x00010000,
    RubbersOptionThreadingAlways      = 0x00020000,

    RubbersOptionWindowStandard       = 0x00000000,
    RubbersOptionWindowShort          = 0x00100000,
    RubbersOptionWindowLong           = 0x00200000,

    RubbersOptionSmoothingOff         = 0x00000000,
    RubbersOptionSmoothingOn          = 0x00800000,

    RubbersOptionFormantShifted       = 0x00000000,
    RubbersOptionFormantPreserved     = 0x01000000,

    RubbersOptionPitchHighQuality     = 0x00000000,
    RubbersOptionPitchHighSpeed       = 0x02000000,
    RubbersOptionPitchHighConsistency = 0x04000000,

    RubbersOptionChannelsApart        = 0x00000000,
    RubbersOptionChannelsTogether     = 0x10000000,
};

typedef int RubbersOptions;

struct RubbersState_;
typedef struct RubbersState_ *RubbersState;

extern RubbersState rubbers_new(unsigned int sampleRate,
                                      unsigned int channels,
                                      RubbersOptions options,
                                      double initialTimeRatio,
                                      double initialPitchScale);

extern void rubbers_delete(RubbersState);

extern void rubbers_reset(RubbersState);

extern void rubbers_set_time_ratio(RubbersState, double ratio);
extern void rubbers_set_pitch_scale(RubbersState, double scale);

extern double rubbers_get_time_ratio(const RubbersState);
extern double rubbers_get_pitch_scale(const RubbersState);

extern size_t  rubbers_get_latency(const RubbersState);

extern void rubbers_set_transients_option(RubbersState, RubbersOptions options);
extern void rubbers_set_detector_option(RubbersState, RubbersOptions options);
extern void rubbers_set_phase_option(RubbersState, RubbersOptions options);
extern void rubbers_set_formant_option(RubbersState, RubbersOptions options);
extern void rubbers_set_pitch_option(RubbersState, RubbersOptions options);

extern void rubbers_set_expected_input_duration(RubbersState, size_t samples);

extern size_t rubbers_get_samples_required(const RubbersState);

extern void rubbers_set_max_process_size(RubbersState, size_t samples);
extern void rubbers_set_key_frame_map(RubbersState, size_t keyframecount, size_t *from, size_t *to);

extern void rubbers_study(RubbersState, const float *const *input, size_t samples, bool flush);
extern void rubbers_process(RubbersState, const float *const *input, size_t samples,bool flush);

extern ssize_t  rubbers_available(const RubbersState);
extern size_t   rubbers_retrieve(const RubbersState, float *const *output, size_t samples);

extern unsigned int rubbers_get_channel_count(const RubbersState);

extern void rubbers_calculate_stretch(RubbersState);

extern void rubbers_set_debug_level(RubbersState, int level);
extern void rubbers_set_default_debug_level(int level);

#ifdef __cplusplus
}
#endif

#endif
