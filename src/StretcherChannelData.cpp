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

#include "StretcherChannelData.h"

#include "dsp/Resampler.h"

#include "system/Allocators.h"

namespace Rubbers 
{
      
RubbersStretcher::Impl::ChannelData::ChannelData(size_t windowSize,
                                                    size_t fftSize,
                                                    size_t outbufSize){
    std::set<size_t> s;
    construct(s, windowSize, fftSize, outbufSize);
}
RubbersStretcher::Impl::ChannelData::ChannelData(std::initializer_list<size_t> sizes,
        size_t initialWindowSize, size_t initialFftSize,size_t initialOutbufSize){
    auto s = std::set<size_t> (sizes.begin(),sizes.end());
    construct(s,initialWindowSize,initialFftSize,initialOutbufSize);
}
RubbersStretcher::Impl::ChannelData::ChannelData(const std::set<size_t> &sizes,
                                                    size_t initialWindowSize,
                                                    size_t initialFftSize,
                                                    size_t outbufSize)
{construct(sizes, initialWindowSize, initialFftSize, outbufSize);}
void
RubbersStretcher::Impl::ChannelData::construct(const std::set<size_t> &sizes,
                                                  size_t initialWindowSize,
                                                  size_t initialFftSize,
                                                  size_t outbufSize){
    auto maxSize = initialWindowSize * 2;
    if (initialFftSize > maxSize) maxSize = initialFftSize;
//    std::cerr << "ChannelData::construct: initialWindowSize = " << initialWindowSize << ", initialFftSize = " << initialFftSize << ", outbufSize = " << outbufSize << std::endl;
    // std::set is ordered by value
    auto i = sizes.cend();
    if (i != sizes.cbegin()) {
        --i;
        if (*i > maxSize) maxSize = *i;
    }
    // max possible size of the real "half" of freq data
    auto realSize = maxSize / 2 + 1;
//    std::cerr << "ChannelData::construct([" << sizes.size() << "], " << maxSize << ", " << realSize << ", " << outbufSize << ")" << std::endl;
    if (outbufSize < maxSize) outbufSize = maxSize;
    inbuf  = new RingBuffer<float>(maxSize);
    outbuf = new RingBuffer<float>(outbufSize);
    mag = allocate_and_zero<float>(realSize);
    phase = allocate_and_zero<float>(realSize);
    prevPhase = allocate_and_zero<float>(realSize);
    prevError = allocate_and_zero<float>(realSize);
    unwrappedPhase = allocate_and_zero<float>(realSize);
    envelope = allocate_and_zero<float>(realSize);
    fltbuf = allocate_and_zero<float>(maxSize);
    dblbuf = allocate_and_zero<float>(maxSize);
    accumulator = allocate_and_zero<float>(maxSize);
    windowAccumulator = allocate_and_zero<float>(maxSize);
    ms = allocate_and_zero<float>(maxSize);
    interpolator = allocate_and_zero<float>(maxSize);
    interpolatorScale = 0;
    for ( auto size : sizes )
    {
        ffts[size] = std::make_unique<FFT>(size);
        ffts[size]->initFloat();
    }
    fft = ffts[initialFftSize].get();
    resampler = 0;
    resamplebuf = 0;
    resamplebufSize = 0;
    reset();
    // Avoid dividing opening sample (which will be discarded anyway) by zero
    windowAccumulator[0] = 1.f;
}
void
RubbersStretcher::Impl::ChannelData::setSizes(size_t windowSize,size_t fftSize){
//    std::cerr << "ChannelData::setSizes: windowSize = " << windowSize << ", fftSize = " << fftSize << std::endl;
    auto  maxSize = 2 * std::max(windowSize, fftSize);
    auto  realSize = maxSize / 2 + 1;
    auto  oldMax = static_cast<decltype(maxSize)>(inbuf->size());
    auto  oldReal = oldMax / 2 + 1;
    if (oldMax >= maxSize) {
        // no need to reallocate buffers, just reselect fft
        //!!! we can't actually do this without locking against the
        //process thread, can we?  we need to zero the mag/phase
        //buffers without interference
        if (ffts.find(fftSize) == ffts.end()) {
            //!!! this also requires a lock, but it shouldn't occur in
            //RT mode with proper initialisation
            ffts[fftSize] = std::make_unique<FFT>(fftSize);
            ffts[fftSize]->initFloat();
        }
        fft = ffts[fftSize].get();
        v_zero(fltbuf, maxSize);
        v_zero(dblbuf, maxSize);
        v_zero(mag, realSize);
        v_zero(phase, realSize);
        v_zero(prevPhase, realSize);
        v_zero(prevError, realSize);
        v_zero(unwrappedPhase, realSize);
        return;
    }
    //!!! at this point we need a lock in case a different client
    //thread is calling process() -- we need this lock even if we
    //aren't running in threaded mode ourselves -- if we're in RT
    //mode, then the process call should trylock and fail if the lock
    //is unavailable (since this should never normally be the case in
    //general use in RT mode)
    auto newbuf = inbuf->resized(maxSize);
    delete inbuf;
    inbuf = newbuf;
    // We don't want to preserve data in these arrays
    mag = reallocate_and_zero(mag, oldReal, realSize);
    phase = reallocate_and_zero(phase, oldReal, realSize);
    prevPhase = reallocate_and_zero(prevPhase, oldReal, realSize);
    prevError = reallocate_and_zero(prevError, oldReal, realSize);
    unwrappedPhase = reallocate_and_zero(unwrappedPhase, oldReal, realSize);
    envelope = reallocate_and_zero(envelope, oldReal, realSize);
    fltbuf = reallocate_and_zero(fltbuf, oldMax, maxSize);
    dblbuf = reallocate_and_zero(dblbuf, oldMax, maxSize);
    ms = reallocate_and_zero(ms, oldMax, maxSize);
    interpolator = reallocate_and_zero(interpolator, oldMax, maxSize);
    // But we do want to preserve data in these
    accumulator = reallocate_and_zero_extension (accumulator, oldMax, maxSize);
    windowAccumulator = reallocate_and_zero_extension (windowAccumulator, oldMax, maxSize);
    interpolatorScale = 0;
    //!!! and resampler?
    if (ffts.find(fftSize) == ffts.end()) {
        ffts[fftSize] = std::make_unique<FFT>(fftSize);
        ffts[fftSize]->initFloat();
    }
    fft = ffts[fftSize].get();
}
void
RubbersStretcher::Impl::ChannelData::setOutbufSize(size_t outbufSize){
    auto oldSize = static_cast<decltype(outbufSize)>(outbuf->size());
//    std::cerr << "ChannelData::setOutbufSize(" << outbufSize << ") [from " << oldSize << "]" << std::endl;
    if (oldSize < outbufSize) {
        //!!! at this point we need a lock in case a different client
        //thread is calling process()
        auto newbuf = outbuf->resized(outbufSize);
        delete outbuf;
        outbuf = newbuf;
    }
}
void
RubbersStretcher::Impl::ChannelData::setResampleBufSize(size_t sz){
    resamplebuf = reallocate_and_zero<float>(resamplebuf, resamplebufSize, sz);
    resamplebufSize = sz;
}
RubbersStretcher::Impl::ChannelData::~ChannelData(){
    delete resampler;
    deallocate(resamplebuf);
    delete inbuf;
    delete outbuf;
    deallocate(mag);
    deallocate(phase);
    deallocate(prevPhase);
    deallocate(prevError);
    deallocate(unwrappedPhase);
    deallocate(envelope);
    deallocate(interpolator);
    deallocate(ms);
    deallocate(accumulator);
    deallocate(windowAccumulator);
    deallocate(fltbuf);
    deallocate(dblbuf);
}
void
RubbersStretcher::Impl::ChannelData::reset(){
    inbuf->reset();
    outbuf->reset();
    if (resampler) resampler->reset();
    auto size = inbuf->size();
    std::fill_n ( accumulator, size, 0 );
    std::fill_n ( windowAccumulator, size, 0 );
    // Avoid dividing opening sample (which will be discarded anyway) by zero
    windowAccumulator[0] = 1.f;
    accumulatorFill = 0;
    prevIncrement = 0;
    chunkCount = 0;
    inCount = 0;
    inputSize = -1;
    outCount = 0;
    interpolatorScale = 0;
    unchanged = true;
    draining = false;
    outputComplete = false;
}
}
