/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

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

#include "Resampler.h"
#include "base/Profiler.h"
#include "system/VectorOps.h"
#include <memory>
#include <cstdlib>
#include <cmath>

#include <iostream>

#ifdef HAVE_LIBSAMPLERATE
#include <samplerate.h>
#endif
#ifdef HAVE_LIBRESAMPLE
#include <libresample.h>
#endif
#ifndef HAVE_LIBSAMPLERATE
#ifndef HAVE_LIBRESAMPLE
#error No resampler implementation selected!
#endif
#endif

namespace RubberBand {

class ResamplerImpl{
public:
    virtual ~ResamplerImpl() { }
    virtual int resample(const float *const *const  in, 
                         float *const  *const  out,
                         int incount,
                         float ratio,
                         bool flush) = 0;
    virtual int getChannelCount() const = 0;
    virtual void reset() = 0;
};
namespace Resamplers {
#ifdef HAVE_LIBSAMPLERATE

class D_SRC : public ResamplerImpl{
public:
    D_SRC(Resampler::Quality quality, int channels, int maxBufferSize,
          int m_debugLevel);
    virtual ~D_SRC();

    int resample(const float *const  *const  in,
                 float *const  *const  out,
                 int incount,
                 float ratio,
                 bool flush);

    int getChannelCount() const { return m_channels; }

    void reset();

protected:
    SRC_STATE *m_src;
    std::unique_ptr<float[]> m_iin;
    std::unique_ptr<float[]> m_iout;
    float m_lastRatio;
    int m_channels;
    int m_iinsize;
    int m_ioutsize;
    int m_debugLevel;
};

D_SRC::D_SRC(Resampler::Quality quality, int channels, int maxBufferSize,
             int debugLevel) :
    m_src(nullptr),
    m_iin(nullptr),
    m_iout(nullptr),
    m_lastRatio(1.f),
    m_channels(channels),
    m_iinsize(0),
    m_ioutsize(0),
    m_debugLevel(debugLevel)
{
    if (m_debugLevel > 0) {
        std::cerr << "Resampler::Resampler: using libsamplerate implementation"
                  << std::endl;
    }

    int err = 0;
    m_src = src_new(quality == Resampler::Best ? SRC_SINC_BEST_QUALITY :
                    quality == Resampler::Fastest ? SRC_LINEAR :
                    SRC_SINC_FASTEST,
                    channels, &err);

    if (err) {
        std::cerr << "Resampler::Resampler: failed to create libsamplerate resampler: " 
                  << src_strerror(err) << std::endl;
#ifndef NO_EXCEPTIONS
        throw Resampler::ImplementationError;
#endif
    }
    if (maxBufferSize > 0 && m_channels > 1) {
        m_iinsize = maxBufferSize * m_channels;
        m_ioutsize = maxBufferSize * m_channels * 2;
        m_iin = std::make_unique<float[]>(m_iinsize);
        m_iout = std::make_unique<float[]>(m_ioutsize);
    }
    reset();
}
D_SRC::~D_SRC(){src_delete(m_src);}
int
D_SRC::resample(const float *const  *const  in,
                float *const  *const  out,
                int incount,
                float ratio,
                bool flush){
    SRC_DATA data;
    int outcount = lrintf(ceilf(incount * ratio));
    if (m_channels == 1) {
        data.data_in = const_cast<float *>(*in); //!!!???
        data.data_out = *out;
    } else {
        if (incount * m_channels > m_iinsize) {
            m_iin = std::make_unique<float[]>(  incount * m_channels);
            m_iinsize = incount * m_channels;
        }
        if (outcount * m_channels > m_ioutsize) {
            m_iout = std::make_unique<float[]>(  outcount * m_channels);
            m_ioutsize = outcount * m_channels;
        }
        v_interleave(m_iin.get(), in, m_channels, incount);
        data.data_in = m_iin.get();
        data.data_out = m_iout.get();
    }
    data.input_frames = incount;
    data.output_frames = outcount;
    data.src_ratio = ratio;
    data.end_of_input = (flush? 1 : 0);
    int err = src_process(m_src, &data);
    if (err) {
        std::cerr << "Resampler::process: libsamplerate error: "<< src_strerror(err) << std::endl;
#ifndef NO_EXCEPTIONS
        throw Resampler::ImplementationError;
#endif
    }
    if (m_channels > 1) {v_deinterleave(out, m_iout.get(), m_channels, data.output_frames_gen);}
    m_lastRatio = ratio;
    return data.output_frames_gen;
}
void
D_SRC::reset(){src_reset(m_src);}

#endif /* HAVE_LIBSAMPLERATE */
#ifdef HAVE_LIBRESAMPLE
class D_Resample : public ResamplerImpl{
public:
    D_Resample(Resampler::Quality quality, int channels, int maxBufferSize, int m_debugLevel);
    virtual ~D_Resample();
    int resample(const float *const  *const  in,
                 float *const  *const  out,
                 int incount,
                 float ratio,
                 bool flush);
    int getChannelCount() const { return m_channels; }
    void reset();

protected:
    void *m_src;
    std::unique_ptr<float[]> m_iin;
    std::unique_ptr<float[]>m_iout;
    float m_lastRatio;
    int m_channels;
    int m_iinsize;
    int m_ioutsize;
    int m_debugLevel;
};

D_Resample::D_Resample(Resampler::Quality quality, int channels, int maxBufferSize,
             int debugLevel) :
    m_src(nullptr),
    m_iin(nullptr),
    m_iout(nullptr),
    m_lastRatio(1.f),
    m_channels(channels),
    m_iinsize(0),
    m_ioutsize(0),
    m_debugLevel(debugLevel)
    if (m_debugLevel > 0) {
        std::cerr << "Resampler::Resampler: using libresample implementation" << std::endl;
    }
    float min_factor = 0.125f;
    float max_factor = 8.0f;
    m_src = resample_open(quality == Resampler::Best ? 1 : 0, min_factor, max_factor);
    if (!m_src) {
        std::cerr << "Resampler::Resampler: failed to create libresample resampler: "  << std::endl;
        throw Resampler::ImplementationError; //!!! of course, need to catch this!
    }
    if (maxBufferSize > 0 && m_channels > 1) {
        m_iinsize = maxBufferSize * m_channels;
        m_ioutsize = maxBufferSize * m_channels * 2;
        m_iin = std::make_unique<float[]>(m_iinsize);;
        m_iout = std::make_unique<float[]>(m_ioutsize);
    }
    reset();
}
D_Resample::~D_Resample(){resample_close(m_src);}
int
D_Resample::resample(const float *const  *const  in,
                     float *const  *const  out,
                     int incount,
                     float ratio,
                     bool flush){
    float *data_in;
    float *data_out;
    int input_frames, output_frames, end_of_input, source_used;
    float src_ratio;
    int outcount = lrintf(ceilf(incount * ratio));
    if (m_channels == 1) {
        data_in = const_cast<float *>(*in); //!!!???
        data_out = *out;
    } else {
        if (incount * m_channels > m_iinsize) {
            m_iin = std::make_unique<float[]>( incount * m_channels);
            m_iinsize = incount * m_channels;
        }
        if (outcount * m_channels > m_ioutsize) {
            m_iout = std:;make_unique<float[]>(outcount * m_channels);
            m_ioutsize = outcount * m_channels;
        }
        v_interleave(m_iin, in, m_channels, incount);
        data_in = m_iin;
        data_out = m_iout;
    }
    input_frames = incount;
    output_frames = outcount;
    src_ratio = ratio;
    end_of_input = (flush? 1 : 0);
    int output_frames_gen = resample_process(m_src,
                                             src_ratio,
                                             data_in,
                                             input_frames,
                                             end_of_input,
                                             &source_used,
                                             data_out,
                                             output_frames);

    if (output_frames_gen < 0) {
        std::cerr << "Resampler::process: libresample error: "
                  << std::endl;
        throw Resampler::ImplementationError; //!!! of course, need to catch this!
    }
    if (m_channels > 1) {
        v_deinterleave(out, m_iout, m_channels, output_frames_gen);
    }
    m_lastRatio = ratio;
    return output_frames_gen;
}

void
D_Resample::reset()
{}
#endif /* HAVE_LIBRESAMPLE */
} /* end namespace Resamplers */
Resampler::Resampler(Resampler::Quality quality, int channels,int maxBufferSize, int debugLevel){
    m_method = -1;
    switch (quality) {
    case Resampler::Best:
#ifdef HAVE_LIBRESAMPLE
        m_method = 3;
#endif
#ifdef HAVE_LIBSAMPLERATE
        m_method = 1;
#endif
        break;
    case Resampler::FastestTolerable:
#ifdef HAVE_LIBRESAMPLE
        m_method = 3;
#endif
#ifdef HAVE_LIBSAMPLERATE
        m_method = 1;
#endif
        break;

    case Resampler::Fastest:
#ifdef HAVE_LIBRESAMPLE
        m_method = 3;
#endif
#ifdef HAVE_LIBSAMPLERATE
        m_method = 1;
#endif
        break;
    }

    if (m_method == -1) {
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize << "): No implementation available!"
                  << std::endl;
        abort();
    }

    switch (m_method) {
    case 0:
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize << "): No implementation available!"
                  << std::endl;
        abort();
        break;

    case 1:
#ifdef HAVE_LIBSAMPLERATE
        d.reset(new Resamplers::D_SRC(quality, channels, maxBufferSize, debugLevel));
#else
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize << "): No implementation available!"
                  << std::endl;
        abort();
#endif
        break;

    case 2:
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize << "): No implementation available!"
                  << std::endl;
        abort();
        break;

    case 3:
#ifdef HAVE_LIBRESAMPLE
        d.reset(new Resamplers::D_Resample(quality, channels, maxBufferSize, debugLevel));
#else
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize << "): No implementation available!"
                  << std::endl;
        abort();
#endif
        break;
    }

    if (!d) {
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize
                  << "): Internal error: No implementation selected"
                  << std::endl;
        abort();
    }
}

Resampler::~Resampler(){}
int 
Resampler::resample(const float *const  *in,float *const *const out,int incount, float ratio, bool flush){
    Profiler profiler("Resampler::resample");
    return d->resample(in, out, incount, ratio, flush);
}
int
Resampler::getChannelCount() const{return d->getChannelCount();}
void
Resampler::reset(){d->reset();}

}
