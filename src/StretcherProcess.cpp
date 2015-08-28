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

#include "audiocurves/PercussiveAudioCurve.h"
#include "audiocurves/HighFrequencyAudioCurve.h"
#include "audiocurves/ConstantAudioCurve.h"

#include "StretchCalculator.h"
#include "StretcherChannelData.h"

#include "dsp/Resampler.h"
#include "base/Profiler.h"
#include "system/VectorOps.h"

#ifndef _WIN32
#include <alloca.h>
#endif

#include <cassert>
#include <cmath>
#include <set>
#include <map>
#include <deque>

using namespace RubberBand;

using std::cerr;
using std::endl;

namespace RubberBand {



void
RubberBandStretcher::Impl::prepareChannelMS(size_t c,
                                            const float *const *inputs,
                                            size_t offset,
                                            size_t samples, 
                                            float *prepared)
{
    if ( c == 0 ){
        for ( auto i = decltype(samples){0}; i < samples; ++i){
            auto left   = inputs[0][i + offset];
            auto right  = inputs[1][i + offset];
            prepared[i] = (left + right) * 0.5f;
        }
    }else{
        for ( auto i = decltype(samples){0};i<samples; ++i){
            auto left  = inputs[0][i + offset];
            auto right = inputs[1][i + offset];
            prepared[i] = (left - right) * 0.5f;
        }
    }
}
size_t
RubberBandStretcher::Impl::consumeChannel(size_t c,
                                          const float *const *inputs,
                                          size_t offset,
                                          size_t samples,
                                          bool final)
{
    Profiler profiler("RubberBandStretcher::Impl::consumeChannel");
    auto &cd = *m_channelData[c];
    auto &inbuf = *cd.inbuf;
    auto toWrite = samples;
    auto writable = static_cast<size_t>(inbuf.getWriteSpace());
    auto resampling = resampleBeforeStretching();
    const float *input = 0;
    auto useMidSide = ((m_options & OptionChannelsTogether) && (m_channels >= 2) && (c < 2));
    if (resampling) {
        toWrite = int(ceil(samples / m_pitchScale));
        if (writable < toWrite) {
            samples = int(floor(writable * m_pitchScale));
            if (samples == 0) return 0;
        }
        auto reqSize = static_cast<size_t>((ceil(samples / m_pitchScale)));
        if (reqSize > cd.resamplebufSize) {
            cerr << "WARNING: RubberBandStretcher::Impl::consumeChannel: resizing resampler buffer from "
                 << cd.resamplebufSize << " to " << reqSize << endl;
            cd.setResampleBufSize(reqSize);
        }
        if (useMidSide) {
            prepareChannelMS(c, inputs, offset, samples, cd.ms);
            input = cd.ms;
        } else {input = inputs[c] + offset;}
        toWrite = cd.resampler->resample(&input,&cd.resamplebuf,samples,1.0f / m_pitchScale,final);
    }
    if (writable < toWrite) {
        if (resampling) {return 0;}
        toWrite = writable;
    }
    if (resampling) {
        inbuf.write(cd.resamplebuf, toWrite);
        cd.inCount += samples;
        return samples;
    } else {
        if (useMidSide) {
            prepareChannelMS(c, inputs, offset, toWrite, cd.ms);
            input = cd.ms;
        } else {input = inputs[c] + offset;}
        inbuf.write(input, toWrite);
        cd.inCount += toWrite;
        return toWrite;
    }
}
void
RubberBandStretcher::Impl::processChunks(size_t c, bool &any, bool &last){
    Profiler profiler("RubberBandStretcher::Impl::processChunks");
    // Process as many chunks as there are available on the input
    // buffer for channel c.  This requires that the increments have
    // already been calculated.
    // This is the normal process method in offline mode.
    auto &cd = *m_channelData[c];
    last = false;
    any = false;
    float *tmp = 0;
    while (!last) {
        if (!testInbufReadSpace(c)) {
            if (m_debugLevel > 2) {cerr << "processChunks: out of input" << endl;}
            break;
        }
        any = true;
        if (!cd.draining) {
            size_t ready = cd.inbuf->getReadSpace();
            assert(ready >= m_aWindowSize || cd.inputSize >= 0);
            cd.inbuf->peek(cd.fltbuf, std::min(ready, m_aWindowSize));
            cd.inbuf->skip(m_increment);
        }
        auto phaseReset = false;
        auto  phaseIncrement = size_t{0}, shiftIncrement = size_t{0};
        getIncrements(c, phaseIncrement, shiftIncrement, phaseReset);
        if (shiftIncrement <= m_aWindowSize) {
            analyseChunk(c);
            last = processChunkForChannel(c, phaseIncrement, shiftIncrement, phaseReset);
        } else {
            auto bit = m_aWindowSize/4;
            if (m_debugLevel > 1) {
                cerr << "channel " << c << " breaking down overlong increment " << shiftIncrement << " into " << bit << "-size bits" << endl;
            }
            if (!tmp) tmp = allocate<float>(m_aWindowSize);
            analyseChunk(c);
            std::copy_n ( cd.fltbuf, m_aWindowSize, tmp );
            for (size_t i = 0; i < shiftIncrement; i += bit) {
                std::copy_n ( tmp, m_aWindowSize, cd.fltbuf );
                size_t thisIncrement = bit;
                if (i + thisIncrement > shiftIncrement) {thisIncrement = shiftIncrement - i;}
                last = processChunkForChannel(c, phaseIncrement + i, thisIncrement, phaseReset);
                phaseReset = false;
            }
        }
        cd.chunkCount++;
        if (m_debugLevel > 2) {cerr << "channel " << c << ": last = " << last << ", chunkCount = " << cd.chunkCount << endl;}
    }
    if (tmp) deallocate(tmp);
}
bool
RubberBandStretcher::Impl::processOneChunk(){
    Profiler profiler("RubberBandStretcher::Impl::processOneChunk");
    // Process a single chunk for all channels, provided there is
    // enough data on each channel for at least one chunk.  This is
    // able to calculate increments as it goes along.
    // This is the normal process method in RT mode.
    for (size_t c = 0; c < m_channels; ++c) {
        if (!testInbufReadSpace(c)) {
            if (m_debugLevel > 2) {cerr << "processOneChunk: out of input" << endl;}
            return false;
        }
        auto &cd = *m_channelData[c];
        if (!cd.draining) {
            size_t ready = cd.inbuf->getReadSpace();
            assert(ready >= m_aWindowSize || cd.inputSize >= 0);
            cd.inbuf->peek(cd.fltbuf, std::min(ready, m_aWindowSize));
            cd.inbuf->skip(m_increment);
            analyseChunk(c);
        }
    }
    auto phaseReset = false;
    auto phaseIncrement = size_t{0}, shiftIncrement = size_t{0};
    if (!getIncrements(0, phaseIncrement, shiftIncrement, phaseReset)) 
    {calculateIncrements(phaseIncrement, shiftIncrement, phaseReset);}
    auto last = false;
    for (size_t c = 0; c < m_channels; ++c) {
        last = processChunkForChannel(c, phaseIncrement, shiftIncrement, phaseReset);
        m_channelData[c]->chunkCount++;
    }
    return last;
}

bool
RubberBandStretcher::Impl::testInbufReadSpace(size_t c){
    Profiler profiler("RubberBandStretcher::Impl::testInbufReadSpace");
    auto &cd = *m_channelData[c];
    auto &inbuf = *cd.inbuf;
    auto rs = inbuf.getReadSpace();
    if (rs < m_aWindowSize && !cd.draining) {
        if (cd.inputSize == -1) {
            // Not all the input data has been written to the inbuf
            // (that's why the input size is not yet set).  We can't
            // process, because we don't have a full chunk of data, so
            // our process chunk would contain some empty padding in
            // its input -- and that would give incorrect output, as
            // we know there is more input to come.
                if (m_debugLevel > 1) {
                    cerr << "WARNING: RubberBandStretcher: read space < chunk size ("
                         << inbuf.getReadSpace() << " < " << m_aWindowSize
                         << ") when not all input written, on processChunks for channel " << c << endl;
                }
            return false;
        }
        if (rs == 0) {
            if (m_debugLevel > 1) {cerr << "read space = 0, giving up" << endl;}
            return false;
        } else if (rs < m_aWindowSize/2) {
            if (m_debugLevel > 1) {cerr << "read space = " << rs << ", setting draining true" << endl;}
            cd.draining = true;
        }
    }
    return true;
}
bool 
RubberBandStretcher::Impl::processChunkForChannel(size_t c,
                                                  size_t phaseIncrement,
                                                  size_t shiftIncrement,
                                                  bool phaseReset)
{
    Profiler profiler("RubberBandStretcher::Impl::processChunkForChannel");
    // Process a single chunk on a single channel.  This assumes
    // enough input data is available; caller must have tested this
    // using e.g. testInbufReadSpace first.  Return true if this is
    // the last chunk on the channel.
    if (phaseReset && (m_debugLevel > 1)) {
        cerr << "processChunkForChannel: phase reset found, incrs " << phaseIncrement << ":" << shiftIncrement << endl;
    }
    auto &cd = *m_channelData[c];
    if (!cd.draining) {
        // This is the normal processing case -- draining is only
        // set when all the input has been used and we only need
        // to write from the existing accumulator into the output.
        // We know we have enough samples available in m_inbuf --
        // this is usually m_aWindowSize, but we know that if fewer
        // are available, it's OK to use zeroes for the rest
        // (which the ring buffer will provide) because we've
        // reached the true end of the data.
        // We need to peek m_aWindowSize samples for processing, and
        // then skip m_increment to advance the read pointer.
        modifyChunk(c, phaseIncrement, phaseReset);
        synthesiseChunk(c, shiftIncrement); // reads from cd.mag, cd.phase

    }
    auto last = false;
    if (cd.draining) {
        if (m_debugLevel > 1) {cerr << "draining: accumulator fill = " << cd.accumulatorFill << " (shiftIncrement = " << shiftIncrement << ")" <<  endl;}
        if (shiftIncrement == 0) {
            cerr << "WARNING: draining: shiftIncrement == 0, can't handle that in this context: setting to " << m_increment << endl;
            shiftIncrement = m_increment;
        }
        if (cd.accumulatorFill <= shiftIncrement) {
            if (m_debugLevel > 1) {cerr << "reducing shift increment from " << shiftIncrement<< " to " << cd.accumulatorFill<< " and marking as last" << endl;}
            shiftIncrement = cd.accumulatorFill;
            last = true;
        }
    }
    auto required = static_cast<ssize_t>(shiftIncrement);
    if (m_pitchScale != 1.0) {required = static_cast<ssize_t>((required / m_pitchScale) + 1);}
    auto ws = static_cast<ssize_t>(cd.outbuf->getWriteSpace());
    if (ws < required) {
        if (m_debugLevel > 0) {cerr << "Buffer overrun on output for channel " << c << endl;}
        // The only correct thing we can do here is resize the buffer.
        // We can't wait for the client thread to read some data out
        // from the buffer so as to make more space, because the
        // client thread (if we are threaded at all) is probably stuck
        // in a process() call waiting for us to stow away enough
        // input increments to allow the process() call to complete.
        // This is an unhappy situation.
        auto *oldbuf = cd.outbuf;
        cd.outbuf = oldbuf->resized(oldbuf->getSize() + (required - ws));
        m_emergencyScavenger.claim(oldbuf);
    }
    writeChunk(c, shiftIncrement, last);
    return last;
}
void
RubberBandStretcher::Impl::calculateIncrements(size_t &phaseIncrementRtn,size_t &shiftIncrementRtn,bool &phaseReset){
    Profiler profiler("RubberBandStretcher::Impl::calculateIncrements");
//    cerr << "calculateIncrements" << endl;
    
    // Calculate the next upcoming phase and shift increment, on the
    // basis that both channels are in sync.  This is in contrast to
    // getIncrements, which requires that all the increments have been
    // calculated in advance but can then return increments
    // corresponding to different chunks in different channels.

    // Requires frequency domain representations of channel data in
    // the mag and phase buffers in the channel.
    // This function is only used in real-time mode.
    phaseIncrementRtn = m_increment;
    shiftIncrementRtn = m_increment;
    phaseReset = false;
    if (m_channels == 0) return;
    auto &cd = *m_channelData[0];
    auto bc = cd.chunkCount;
    for (size_t c = 1; c < m_channels; ++c) {
        if (m_channelData[c]->chunkCount != bc) {
            cerr << "ERROR: RubberBandStretcher::Impl::calculateIncrements: Channels are not in sync" << endl;
            return;
        }
    }
    const auto hs = m_fftSize/2 + 1;
    // Normally we would mix down the time-domain signal and apply a
    // single FFT, or else mix down the Cartesian form of the
    // frequency-domain signal.  Both of those would be inefficient
    // from this position.  Fortunately, the onset detectors should
    // work reasonably well (maybe even better?) if we just sum the
    // magnitudes of the frequency-domain channel signals and forget
    // about phase entirely.  Normally we don't expect the channel
    // phases to cancel each other, and broadband effects will still
    // be apparent.
    auto df = 0.f;
    auto silent = false;
    if (m_channels == 1) {
        df = m_phaseResetAudioCurve->process((float *)cd.mag, m_increment);
        silent = (m_silentAudioCurve->process((float *)cd.mag, m_increment) > 0.f);
    } else {
        auto tmp = (float *)alloca(hs * sizeof(float));
        std::fill_n ( tmp, hs, 0.f );
        for (auto c = decltype(m_channels){0}; c < m_channels; ++c) {
            v_add(tmp, m_channelData[c]->mag, hs);
        }
        df = m_phaseResetAudioCurve->process((float *)tmp, m_increment);
        silent = (m_silentAudioCurve->process((float *)tmp, m_increment) > 0.f);
    }
    auto incr = m_stretchCalculator->calculateSingle(getEffectiveRatio(), df, m_increment);
    if (m_lastProcessPhaseResetDf.getWriteSpace() > 0) {m_lastProcessPhaseResetDf.write(&df, 1);}
    if (m_lastProcessOutputIncrements.getWriteSpace() > 0) {m_lastProcessOutputIncrements.write(&incr, 1);}
    if (incr < 0) {
        phaseReset = true;
        incr = -incr;
    }
    // The returned increment is the phase increment.  The shift
    // increment for one chunk is the same as the phase increment for
    // the following chunk (see comment below).  This means we don't
    // actually know the shift increment until we see the following
    // phase increment... which is a bit of a problem.

    // This implies we should use this increment for the shift
    // increment, and make the following phase increment the same as
    // it.  This means in RT mode we'll be one chunk later with our
    // phase reset than we would be in non-RT mode.  The sensitivity
    // of the broadband onset detector may mean that this isn't a
    // problem -- test it and see.
    shiftIncrementRtn = incr;
    if (cd.prevIncrement == 0) {phaseIncrementRtn = shiftIncrementRtn;}
    else {phaseIncrementRtn = cd.prevIncrement;}
    cd.prevIncrement = shiftIncrementRtn;
    if (silent) ++m_silentHistory;
    else m_silentHistory = 0;
    if (m_silentHistory >= int(m_aWindowSize / m_increment) && !phaseReset) {
        phaseReset = true;
        if (m_debugLevel > 1) {
            cerr << "calculateIncrements: phase reset on silence (silent history == " << m_silentHistory << ")" << endl;
        }
    }
}
bool
RubberBandStretcher::Impl::getIncrements(size_t channel,size_t &phaseIncrementRtn,size_t &shiftIncrementRtn,bool &phaseReset){
    Profiler profiler("RubberBandStretcher::Impl::getIncrements");
    if (channel >= m_channels) {
        phaseIncrementRtn = m_increment;
        shiftIncrementRtn = m_increment;
        phaseReset = false;
        return false;
    }
    // There are two relevant output increments here.  The first is
    // the phase increment which we use when recalculating the phases
    // for the current chunk; the second is the shift increment used
    // to determine how far to shift the processing buffer after
    // writing the chunk.  The shift increment for one chunk is the
    // same as the phase increment for the following chunk.
    
    // When an onset occurs for which we need to reset phases, the
    // increment given will be negative.
    
    // When we reset phases, the previous shift increment (and so
    // current phase increments) must have been m_increment to ensure
    // consistency.
    
    // m_outputIncrements stores phase increments.
    auto &cd = *m_channelData[channel];
    auto gotData = true;
    if (cd.chunkCount >= m_outputIncrements.size()) {
//        cerr << "WARNING: RubberBandStretcher::Impl::getIncrements:"
//             << " chunk count " << cd.chunkCount << " >= "
//             << m_outputIncrements.size() << endl;
        if (m_outputIncrements.size() == 0) {
            phaseIncrementRtn = m_increment;
            shiftIncrementRtn = m_increment;
            phaseReset = false;
            return false;
        } else {
            cd.chunkCount = m_outputIncrements.size()-1;
            gotData = false;
        }
    }
    auto phaseIncrement = m_outputIncrements[cd.chunkCount];
    auto shiftIncrement = phaseIncrement;
    if (cd.chunkCount + 1 < m_outputIncrements.size()) 
    {shiftIncrement = m_outputIncrements[cd.chunkCount + 1];}
    if (phaseIncrement < 0) {
        phaseIncrement = -phaseIncrement;
        phaseReset = true;
    }
    if (shiftIncrement < 0) {shiftIncrement = -shiftIncrement;}
    /*
    if (shiftIncrement >= int(m_windowSize)) {
        cerr << "*** ERROR: RubberBandStretcher::Impl::processChunks: shiftIncrement " << shiftIncrement << " >= windowSize " << m_windowSize << " at " << cd.chunkCount << " (of " << m_outputIncrements.size() << ")" << endl;
        shiftIncrement = m_windowSize;
    }
    */
    phaseIncrementRtn = phaseIncrement;
    shiftIncrementRtn = shiftIncrement;
    if (cd.chunkCount == 0) phaseReset = true; // don't mess with the first chunk
    return gotData;
}

void
RubberBandStretcher::Impl::analyseChunk(size_t channel){
    Profiler profiler("RubberBandStretcher::Impl::analyseChunk");
    ChannelData &cd = *m_channelData[channel];
    float *const  dblbuf = cd.dblbuf;
    float *const  fltbuf = cd.fltbuf;
    // cd.fltbuf is known to contain m_aWindowSize samples
    if (m_aWindowSize > m_fftSize) {m_afilter->cut(fltbuf);}
    cutShiftAndFold(dblbuf, m_fftSize, fltbuf, m_awindow);
    cd.fft->forwardPolar(dblbuf, cd.mag, cd.phase);
}

void
RubberBandStretcher::Impl::modifyChunk(size_t channel,size_t outputIncrement,bool phaseReset){
    Profiler profiler("RubberBandStretcher::Impl::modifyChunk");
    ChannelData &cd = *m_channelData[channel];
    if (phaseReset && m_debugLevel > 1) {cerr << "phase reset: leaving phases unmodified" << endl;}
    const auto rate = m_sampleRate;
    const auto count = m_fftSize / 2;
    auto unchanged = cd.unchanged && (outputIncrement == m_increment);
    auto fullReset = phaseReset;
    auto laminar = !(m_options & OptionPhaseIndependent);
    auto bandlimited = (m_options & OptionTransientsMixed);
    auto bandlow = static_cast<decltype(count)>(lrint((150 * m_fftSize) / rate));
    auto bandhigh = static_cast<decltype(count)>(lrint((1000 * m_fftSize) / rate));
    auto freq0 = m_freq0;
    auto freq1 = m_freq1;
    auto freq2 = m_freq2;
    if (laminar) {
        auto r = static_cast<float>(getEffectiveRatio());
        if (r > 1) {
            auto  rf0 = 600 + (600 * ((r-1)*(r-1)*(r-1)*2));
            auto  f1ratio = freq1 / freq0;
            auto  f2ratio = freq2 / freq0;
            freq0 = std::max(freq0, rf0);
            freq1 = freq0 * f1ratio;
            freq2 = freq0 * f2ratio;
        }
    }
    auto limit0 = static_cast<decltype(count)>(lrint((freq0 * m_fftSize) / rate));
    auto limit1 = static_cast<decltype(count)>(lrint((freq1 * m_fftSize) / rate));
    auto limit2 = static_cast<decltype(count)>(lrint((freq2 * m_fftSize) / rate));
    if (limit1 < limit0) limit1 = limit0;
    if (limit2 < limit1) limit2 = limit1;
    auto prevInstability = 0.0f;
    auto prevDirection = false;
    auto distance = 0.0f;
    const auto maxdist = 8.0f;
    const auto lookback = 1;
    auto distacc = 0.0f;
    for (auto i = static_cast<int>(count); i >= 0; i -= lookback) {
        auto resetThis = phaseReset;
        if (bandlimited) {
            if (resetThis) {
                if (i > bandlow && i < bandhigh) {
                    resetThis = false;
                    fullReset = false;
                }
            }
        }
        auto p = cd.phase[i];
        auto perr = 0.0f;
        auto outphase = p;
        auto mi = maxdist;
        if (i <= limit0) mi = 0.0f;
        else if (i <= limit1) mi = 1.0f;
        else if (i <= limit2) mi = 3.0f;
        if (!resetThis) {
            auto omega = (static_cast<float>(2 * M_PI) * m_increment * i) / (m_fftSize);
            auto pp = cd.prevPhase[i];
            auto ep = pp + omega;
            perr = princarg(p - ep);
            auto instability = fabs(perr - cd.prevError[i]);
            auto direction = (perr > cd.prevError[i]);
            auto inherit = false;
            if (laminar) {
                if (distance >= mi || i == count) {inherit = false;}
                else if (bandlimited && (i == bandhigh || i == bandlow)) {inherit = false;}
                else if (instability > prevInstability &&direction == prevDirection) {inherit = true;}
            }
            auto advance = outputIncrement * ((omega + perr) / m_increment);
            if (inherit) {
                auto inherited = cd.unwrappedPhase[i + lookback] - cd.prevPhase[i + lookback];
                advance = ((advance * distance) + (inherited * (maxdist - distance))) / maxdist;
                outphase = p + advance;
                distacc += distance;
                distance += 1;
            } else {
                outphase = cd.unwrappedPhase[i] + advance;
                distance = 0;
            }
            prevInstability = instability;
            prevDirection = direction;
        } else {distance = 0.0;}
        cd.prevError[i] = perr;
        cd.prevPhase[i] = p;
        cd.phase[i] = outphase;
        cd.unwrappedPhase[i] = outphase;
    }
    if (m_debugLevel > 2) {cerr << "mean inheritance distance = " << distacc / count << endl;}
    if (fullReset) unchanged = true;
    cd.unchanged = unchanged;
    if (unchanged && m_debugLevel > 1) {cerr << "frame unchanged on channel " << channel << endl;}
}    
void
RubberBandStretcher::Impl::formantShiftChunk(size_t channel){
    Profiler profiler("RubberBandStretcher::Impl::formantShiftChunk");
    auto &cd = *m_channelData[channel];
    float *const  mag = cd.mag;
    float *const  envelope = cd.envelope;
    float *const  dblbuf = cd.dblbuf;
    const auto  sz = m_fftSize;
    const auto  hs = sz / 2;
    const auto  factor = 1.0f / sz;
    cd.fft->inverseCepstral(mag, dblbuf);
    const auto cutoff = static_cast<decltype(sz)>(m_sampleRate / 700);
//    cerr <<"cutoff = "<< cutoff << ", m_sampleRate/cutoff = " << m_sampleRate/cutoff << endl;
    dblbuf[0] /= 2;
    dblbuf[cutoff-1] /= 2;
    for (auto i = cutoff; i < sz; ++i) {dblbuf[i] = 0.0;}
    v_scale(dblbuf, factor, cutoff);
    auto spare = (float *)alloca((hs + 1) * sizeof(float));
    cd.fft->forward(dblbuf, envelope, spare);
    v_exp(envelope, hs + 1);
    v_divide(mag, envelope, hs + 1);
    if (m_pitchScale > 1.0) {
        // scaling up, we want a new envelope that is lower by the pitch factor
        for (auto target = decltype(hs){0}; target <= hs; ++target) {
            auto source = static_cast<decltype(hs)>(lrint(target * m_pitchScale));
            if (source > hs) {envelope[target] = 0.0;}
            else {envelope[target] = envelope[source];}
        }
    } else {
        // scaling down, we want a new envelope that is higher by the pitch factor
        for (int target = hs; target > 0; ) {
            --target;
            int source = lrint(target * m_pitchScale);
            envelope[target] = envelope[source];
        }
    }
    v_multiply(mag, envelope, hs+1);
    cd.unchanged = false;
}

void
RubberBandStretcher::Impl::synthesiseChunk(size_t channel,size_t shiftIncrement){
    Profiler profiler("RubberBandStretcher::Impl::synthesiseChunk");
    if ((m_options & OptionFormantPreserved) &&
        (m_pitchScale != 1.0)) {
        formantShiftChunk(channel);
    }
    auto &cd = *m_channelData[channel];
    float *const  dblbuf = cd.dblbuf;
    float *const  fltbuf = cd.fltbuf;
    float *const  accumulator = cd.accumulator;
    float *const  windowAccumulator = cd.windowAccumulator;
    const auto fsz = m_fftSize;
    const auto hs = fsz / 2;
    const auto wsz = m_sWindowSize;
    if (!cd.unchanged) {
        // Our FFTs produced unscaled results. Scale before inverse
        // transform rather than after, to avoid overflow if using a
        // fixed-point FFT.
        auto factor = 1.f / fsz;
        v_scale(cd.mag, factor, hs + 1);
        cd.fft->inversePolar(cd.mag, cd.phase, cd.dblbuf);
        if (wsz == fsz) {v_convert(fltbuf, dblbuf + hs, hs);v_convert(fltbuf + hs, dblbuf, hs);
        } else {
            v_zero(fltbuf, wsz);
            auto j = (fsz - wsz/2);
            while (j < 0) j += fsz;
            for (auto i = decltype(wsz){0}; i < wsz; ++i) {
                fltbuf[i] += dblbuf[j];
                if (++j == fsz) j = 0;
            }
        }
    }
    if (wsz > fsz) {
        auto p = shiftIncrement * 2;
        if (cd.interpolatorScale != p) {
            SincWindow<float>::write(cd.interpolator, wsz, p);
            cd.interpolatorScale = p;
        }
        v_multiply(fltbuf, cd.interpolator, wsz);
    }
    m_swindow->cut(fltbuf);
    v_add(accumulator, fltbuf, wsz);
    cd.accumulatorFill = wsz;
    if (wsz > fsz) {
        // reuse fltbuf to calculate interpolating window shape for
        // window accumulator
        v_copy(fltbuf, cd.interpolator, wsz);
        m_swindow->cut(fltbuf);
        v_add(windowAccumulator, fltbuf, wsz);
    } else {m_swindow->add(windowAccumulator, m_awindow->getArea() * 1.5f);}
}

void
RubberBandStretcher::Impl::writeChunk(size_t channel, size_t shiftIncrement, bool last){
    Profiler profiler("RubberBandStretcher::Impl::writeChunk");
    auto &cd = *m_channelData[channel];
    float *const  accumulator = cd.accumulator;
    float *const  windowAccumulator = cd.windowAccumulator;
    const auto sz = m_sWindowSize;
    const auto si = shiftIncrement;
    if (m_debugLevel > 2) {cerr << "writeChunk(" << channel << ", " << shiftIncrement << ", " << last << ")" << endl;}
    v_divide(accumulator, windowAccumulator, si);
    // for exact sample scaling (probably not meaningful if we
    // were running in RT mode)
    auto theoreticalOut = size_t{0};
    if (cd.inputSize >= 0) {theoreticalOut = lrint(cd.inputSize * m_timeRatio);}
    auto resampledAlready = resampleBeforeStretching();
    if (!resampledAlready &&
        (m_pitchScale != 1.0 || m_options & OptionPitchHighConsistency) &&
        cd.resampler) {
        auto reqSize = static_cast<size_t>(ceil(si / m_pitchScale));
        if (reqSize > cd.resamplebufSize) {
            // This shouldn't normally happen -- the buffer is
            // supposed to be initialised with enough space in the
            // first place.  But we retain this check in case the
            // pitch scale has changed since then, or the stretch
            // calculator has gone mad, or something.
            cerr << "WARNING: RubberBandStretcher::Impl::writeChunk: resizing resampler buffer from "<< cd.resamplebufSize << " to " << reqSize << endl;
            cd.setResampleBufSize(reqSize);
        }
        auto outframes = cd.resampler->resample(&cd.accumulator,&cd.resamplebuf,si,1.0 / m_pitchScale,last);
        writeOutput(*cd.outbuf, cd.resamplebuf,outframes, cd.outCount, theoreticalOut);
    } else {writeOutput(*cd.outbuf, accumulator,si, cd.outCount, theoreticalOut);}
    v_move(accumulator, accumulator + si, sz - si);
    v_zero(accumulator + sz - si, si);
    v_move(windowAccumulator, windowAccumulator + si, sz - si);
    v_zero(windowAccumulator + sz - si, si);
    if (cd.accumulatorFill > si) {cd.accumulatorFill -= si;}
    else {
        cd.accumulatorFill = 0;
        if (cd.draining) {
            if (m_debugLevel > 1) {cerr << "RubberBandStretcher::Impl::processChunks: setting outputComplete to true" << endl;}
            cd.outputComplete = true;
        }
    }
}
void
RubberBandStretcher::Impl::writeOutput(RingBuffer<float> &to, float *from, size_t qty, size_t &outCount, size_t theoreticalOut){
    Profiler profiler("RubberBandStretcher::Impl::writeOutput");
    // In non-RT mode, we don't want to write the first startSkip
    // samples, because the first chunk is centred on the start of the
    // output.  In RT mode we didn't apply any pre-padding in
    // configure(), so we don't want to remove any here.
    auto  startSkip = size_t{0};
    if (!m_realtime) {startSkip = lrintf((m_sWindowSize/2) / m_pitchScale);}
    if (outCount > startSkip) {
        // this is the normal case
        if (theoreticalOut > 0) {
            if (m_debugLevel > 1) {
                cerr << "theoreticalOut = " << theoreticalOut
                     << ", outCount = " << outCount
                     << ", startSkip = " << startSkip
                     << ", qty = " << qty << endl;
            }
            if (outCount - startSkip <= theoreticalOut &&
                outCount - startSkip + qty > theoreticalOut) {
                qty = theoreticalOut - (outCount - startSkip);
                if (m_debugLevel > 1) {
                    cerr << "reduce qty to " << qty << endl;
                }
            }
        }
        if (m_debugLevel > 2) {cerr << "writing " << qty << endl;}
        auto written = to.write(from, qty);
        if (written < qty) {
            cerr << "WARNING: RubberBandStretcher::Impl::writeOutput: "
                 << "Buffer overrun on output: wrote " << written
                 << " of " << qty << " samples" << endl;
        }
        outCount += written;
        return;
    }
    // the rest of this is only used during the first startSkip samples
    if (outCount + qty <= startSkip) {
        if (m_debugLevel > 1) {
            cerr << "qty = " << qty << ", startSkip = "
                 << startSkip << ", outCount = " << outCount
                 << ", discarding" << endl;
        }
        outCount += qty;
        return;
    }
    auto off = startSkip - outCount;
    if (m_debugLevel > 1) {
        cerr << "qty = " << qty << ", startSkip = "
             << startSkip << ", outCount = " << outCount
             << ", writing " << qty - off
             << " from start offset " << off << endl;
    }
    to.write(from + off, qty - off);
    outCount += qty;
}

int
RubberBandStretcher::Impl::available() const{
    Profiler profiler("RubberBandStretcher::Impl::available");
/*        for (size_t c = 0; c < m_channels; ++c) {
            if (m_channelData[c]->inputSize >= 0) {
//                cerr << "available: m_done true" << endl;
                if (m_channelData[c]->inbuf->getReadSpace() > 0) {
                    //!!! do we ever actually do this? if so, this method should not be const
                    // ^^^ yes, we do sometimes -- e.g. when fed a very short file
                    auto any = false, last = false;
                    processChunks(c, any, last);
                }
            }
        }*/
    auto min = size_t{0};
    auto consumed = true;
    auto haveResamplers = false;
    for (size_t i = 0; i < m_channels; ++i) {
        auto availIn = static_cast<size_t>(m_channelData[i]->inbuf->getReadSpace());
        auto availOut = static_cast<size_t>(m_channelData[i]->outbuf->getReadSpace());
        if (m_debugLevel > 2) {cerr << "available on channel " << i << ": " << availOut << " (waiting: " << availIn << ")" << endl;}
        if (i == 0 || availOut < min) min = availOut;
        if (!m_channelData[i]->outputComplete) consumed = false;
        if (m_channelData[i]->resampler) haveResamplers = true;
    }
    if (min == 0 && consumed) return -1;
    if (m_pitchScale == 1.0) return min;
    if (haveResamplers) return min; // resampling has already happened
    return int(floor(min / m_pitchScale));
}
size_t
RubberBandStretcher::Impl::retrieve(float *const *output, size_t samples) const{
    Profiler profiler("RubberBandStretcher::Impl::retrieve");
    auto got = samples;
    for (size_t c = 0; c < m_channels; ++c) {
        auto gotHere = static_cast<size_t>(m_channelData[c]->outbuf->read(output[c], got));
        if (gotHere < got) {
            if (c > 0) {
                if (m_debugLevel > 0) {
                    cerr << "RubberBandStretcher::Impl::retrieve: WARNING: channel imbalance detected" << endl;
                }
            }
            got = gotHere;
        }
    }
    if ((m_options & OptionChannelsTogether) && (m_channels >= 2)) {
        for (size_t i = 0; i < got; ++i) {
            auto mid = output[0][i];
            auto side = output[1][i];
            auto left = mid + side;
            auto right = mid - side;
            output[0][i] = left;
            output[1][i] = right;
        }
    }            
    return got;
}
}

