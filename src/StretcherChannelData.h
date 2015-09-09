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

#ifndef _RUBBERBAND_STRETCHERCHANNELDATA_H_
#define _RUBBERBAND_STRETCHERCHANNELDATA_H_

#include "StretcherImpl.h"

#include <set>

//#define EXPERIMENT 1

namespace Rubbers
{

class Resampler;

class RubbersStretcher::Impl::ChannelData
{
public:        
    /**
     * Construct a ChannelData structure.
     *
     * The sizes passed in here are for the time-domain analysis
     * window and FFT calculation, and most of the buffer sizes also
     * depend on them.  In practice they are always powers of two, the
     * window and FFT sizes are either equal or generally in a 2:1
     * relationship either way, and except for very extreme stretches
     * the FFT size is either 1024, 2048 or 4096.
     *
     * The outbuf size depends on other factors as well, including
     * the pitch scale factor and any maximum processing block
     * size specified by the user of the code.
     */
    ChannelData(size_t windowSize,size_t fftSize,size_t outbufSize);
    /**
     * Construct a ChannelData structure that can process at different
     * FFT sizes without requiring reallocation when the size changes.
     * The sizes can subsequently be changed with a call to setSizes.
     * Reallocation will only be necessary if setSizes is called with
     * values not equal to any of those passed in to the constructor.
     *
     * The outbufSize should be the maximum possible outbufSize to
     * avoid reallocation, which will happen if setOutbufSize is
     * called subsequently.
     */
    ChannelData(std::initializer_list<size_t> sizes, size_t initalWindowSize, size_t initialFFTSize, size_t outbufSize);
    ChannelData(const std::set<size_t> &sizes,size_t initialWindowSize,size_t initialFftSize,size_t outbufSize);
    virtual ~ChannelData();
    /**
     * Reset buffers
     */
    virtual void reset();
    /**
     * Set the FFT, analysis window, and buffer sizes.  If this
     * ChannelData was constructed with a set of sizes and the given
     * window and FFT sizes here were among them, no reallocation will
     * be required.
     */
    virtual void setSizes(size_t windowSize, size_t fftSizes);
    /**
     * Set the outbufSize for the channel data.  Reallocation will
     * occur.
     */
    virtual void setOutbufSize(size_t outbufSize);
    /**
     * Set the resampler buffer size.  Default if not called is no
     * buffer allocated at all.
     */
    virtual void setResampleBufSize(size_t resamplebufSize);
    RingBuffer<float> *inbuf;
    RingBuffer<float> *outbuf;
    float *mag;
    float *phase;
    float *prevPhase;
    float *prevError;
    float *unwrappedPhase;
    float *accumulator;
    size_t accumulatorFill;
    float *windowAccumulator;
    float *ms; // only used when mid-side processing
    float *interpolator; // only used when time-domain smoothing is on
    int interpolatorScale;
    float *fltbuf;
    float *dblbuf; // owned by FFT object, only used for time domain FFT i/o
    float *envelope; // for cepstral formant shift
    bool unchanged;
    size_t prevIncrement; // only used in RT mode
    size_t chunkCount;
    size_t inCount;
    long inputSize; // set only after known (when data ended); -1 previously
    size_t outCount;
    bool draining;
    bool outputComplete;
    FFT *fft;
    std::map<size_t, std::unique_ptr<FFT> > ffts;
    std::unique_ptr<Resampler> resampler;
    float *resamplebuf;
    size_t resamplebufSize;
protected:
    virtual void construct(const std::set<size_t> &sizes,size_t initialWindowSize, size_t initialFftSize,size_t outbufSize);
};        
}
#endif
