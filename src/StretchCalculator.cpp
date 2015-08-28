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

#include "StretchCalculator.h"

#include <math.h>
#include <iostream>
#include <deque>
#include <set>
#include <cassert>
#include <algorithm>

#include "system/sysutils.h"

namespace RubberBand
{
	
StretchCalculator::StretchCalculator(size_t sampleRate,
                                     size_t inputIncrement,
                                     bool useHardPeaks) :
    m_sampleRate(sampleRate),
    m_increment(inputIncrement),
    m_prevDf(0),
    m_divergence(0),
    m_recovery(0),
    m_prevRatio(1.0),
    m_transientAmnesty(0),
    m_debugLevel(0),
    m_useHardPeaks(useHardPeaks)
{
//    std::cerr << "StretchCalculator::StretchCalculator: useHardPeaks = " << useHardPeaks << std::endl;
}    

StretchCalculator::~StretchCalculator(){}
void
StretchCalculator::setKeyFrameMap(const std::map<size_t, size_t> &mapping){
    m_keyFrameMap = mapping;
    // Ensure we always have a 0 -> 0 mapping. If there's nothing in
    // the map at all, don't need to worry about this (empty map is
    // handled separately anyway)
    if (!m_keyFrameMap.empty()) {if (m_keyFrameMap.find(0) == m_keyFrameMap.end()) {m_keyFrameMap[0] = 0;}}
}

std::vector<int>
StretchCalculator::calculate(double ratio, size_t inputDuration,const std::vector<float> &phaseResetDf,const std::vector<float> &stretchDf){
    assert(phaseResetDf.size() == stretchDf.size());
    m_peaks             = findPeaks(phaseResetDf);
    auto totalCount     = phaseResetDf.size();
    auto outputDuration = static_cast<size_t>(lrint(inputDuration * ratio));
    if (m_debugLevel > 0) {
        std::cerr << "StretchCalculator::calculate(): inputDuration " << inputDuration << ", ratio " << ratio << ", outputDuration " << outputDuration;
    }
    outputDuration = lrint((phaseResetDf.size() * m_increment) * ratio);
    if (m_debugLevel > 0) {
        std::cerr << " (rounded up to " << outputDuration << ")";
        std::cerr << ", df size " << phaseResetDf.size() << ", increment "
                  << m_increment << std::endl;
    }
    auto peaks   = std::vector<Peak>{};
    auto targets = std::vector<size_t>{};
    mapPeaks(peaks, targets, outputDuration, totalCount);
    if (m_debugLevel > 1) {std::cerr << "have " << peaks.size() << " fixed positions" << std::endl;}
    auto totalInput = size_t{0}, totalOutput = size_t{0};
    // For each region between two consecutive time sync points, we
    // want to take the number of output chunks to be allocated and
    // the detection function values within the range, and produce a
    // series of increments that sum to the number of output chunks,
    // such that each increment is displaced from the input increment
    // by an amount inversely proportional to the magnitude of the
    // stretch detection function at that input step.
    auto regionTotalChunks = size_t{0};
    auto increments = std::vector<int>{};
    for (auto i = size_t{0}; i <= peaks.size(); ++i) {
        auto regionStart = size_t{0}, regionStartChunk = size_t{0}, regionEnd = size_t{0}, regionEndChunk = size_t{0};
        auto phaseReset = false;
        if (i > 0) {
            regionStartChunk = peaks[i-1].chunk;
            regionStart      = targets[i-1];
            phaseReset       = peaks[i-1].hard;
        }
        if (i == peaks.size()) {
//            std::cerr << "note: i (=" << i << ") == peaks.size(); regionEndChunk " << regionEndChunk << " -> " << totalCount << ", regionEnd " << regionEnd << " -> " << outputDuration << std::endl;
            regionEndChunk = totalCount;
            regionEnd      = outputDuration;
        } else {
            regionEndChunk = peaks[i].chunk;
            regionEnd      = targets[i];
        }
        regionStartChunk = std::min ( regionStartChunk, totalCount     );
        regionStart      = std::min ( regionStart,      outputDuration );
        regionEndChunk   = std::min ( regionEndChunk,   totalCount     );
        regionEnd        = std::min ( regionEnd,        outputDuration );
        auto regionDuration = regionEnd - regionStart;
        regionTotalChunks += regionDuration;
        auto dfRegion = std::vector<float>(stretchDf.cbegin() + regionStartChunk, stretchDf.cbegin()+regionEndChunk);
        if (m_debugLevel > 1) {
            std::cerr << "distributeRegion from " << regionStartChunk << " to " << regionEndChunk << " (samples " << regionStart << " to " << regionEnd << ")" << std::endl;
        }
        dfRegion = smoothDF(dfRegion);
        auto regionIncrements = distributeRegion (dfRegion, regionDuration, ratio, phaseReset);
        auto totalForRegion   = size_t{0};
        for (size_t j = 0; j < regionIncrements.size(); ++j) {
            auto incr = regionIncrements[j];
            if (j == 0 && phaseReset) increments.push_back(-incr);
            else increments.push_back(incr);
            if (incr > 0) totalForRegion += incr;
            else totalForRegion += -incr;
            totalInput += m_increment;
        }
        if (totalForRegion != regionDuration) {
            std::cerr << "*** ERROR: distributeRegion returned wrong duration " << totalForRegion << ", expected " << regionDuration << std::endl;
        }
        totalOutput += totalForRegion;
    }
    if (m_debugLevel > 0) {
        std::cerr << "total input increment = " << totalInput << " (= " << totalInput / m_increment << " chunks), output = " << totalOutput << ", ratio = " << double(totalOutput)/double(totalInput) << ", ideal output " << size_t(ceil(totalInput * ratio)) << std::endl;
        std::cerr << "(region total = " << regionTotalChunks << ")" << std::endl;
    }
    return increments;
}
void
StretchCalculator::mapPeaks(std::vector<Peak> &peaks,
                            std::vector<size_t> &targets,
                            size_t outputDuration,
                            size_t totalCount){
    // outputDuration is in audio samples; totalCount is in chunks
    if (m_keyFrameMap.empty()) {
        // "normal" behaviour -- fixed points are strictly in
        // proportion
        peaks = m_peaks;
        for (size_t i = 0; i < peaks.size(); ++i) {
            targets.push_back (lrint((double(peaks[i].chunk) * outputDuration) / totalCount));
        }
        return;
    }
    // We have been given a set of source -> target sample frames in
    // m_keyFrameMap.  We want to ensure that (to the nearest chunk) these
    // are followed exactly, and any fixed points that we calculated
    // ourselves are interpolated in linear proportion in between.
    auto peakidx = size_t{0};
    auto mi = m_keyFrameMap.cbegin();
    // NB we know for certain we have a mapping from 0 -> 0 (or at
    // least, some mapping for source sample 0) because that is
    // enforced in setLockPoints above.  However, we aren't guaranteed
    // to have a mapping for the total duration -- we will usually
    // need to assume it maps to the normal duration * ratio sample
    while (mi != m_keyFrameMap.cend()) {
//        std::cerr << "mi->first is " << mi->first << ", second is " << mi->second <<std::endl;
        // The map we've been given is from sample to sample, but
        // we can only map from chunk to sample.  We should perhaps
        // adjust the target sample to compensate for the discrepancy
        // between the chunk position and the exact requested source
        // sample.  But we aren't doing that yet.
        auto sourceStartChunk = mi->first / m_increment;
        auto sourceEndChunk = totalCount;
        auto targetStartSample = mi->second;
        auto targetEndSample = outputDuration;
        ++mi;
        if (mi != m_keyFrameMap.end()) {
            sourceEndChunk = mi->first / m_increment;
            targetEndSample = mi->second;
        }
        if (sourceStartChunk >= totalCount ||
            sourceStartChunk >= sourceEndChunk ||
            targetStartSample >= outputDuration ||
            targetStartSample >= targetEndSample) {
            std::cerr << "NOTE: ignoring mapping from chunk " << sourceStartChunk << " to sample " << targetStartSample << "\n(source or target chunk exceeds total count, or end is not later than start)" << std::endl;
            continue;
        }
        // one peak and target for the mapping, then one for each of
        // the computed peaks that appear before the following mapping
        auto p = Peak{sourceStartChunk,false};
        peaks.push_back(p);
        targets.push_back(targetStartSample);
        if (m_debugLevel > 1) {std::cerr << "mapped chunk " << sourceStartChunk << " (frame " << sourceStartChunk * m_increment << ") -> " << targetStartSample << std::endl;}
        while (peakidx < m_peaks.size()) {
            auto pchunk = m_peaks[peakidx].chunk;
            if (pchunk < sourceStartChunk) {
                // shouldn't happen, should have been dealt with
                // already -- but no harm in ignoring it explicitly
                ++peakidx;
                continue;
            }
            if (pchunk == sourceStartChunk) {
                // convert that last peak to a hard one, after all
                peaks[peaks.size()-1].hard = true;
                ++peakidx;
                continue;
            }
            if (pchunk >= sourceEndChunk) {break;}
            p.chunk = pchunk;
            p.hard = m_peaks[peakidx].hard;
            auto proportion =
                double(pchunk - sourceStartChunk) /
                double(sourceEndChunk - sourceStartChunk);
            auto target = static_cast<size_t>(targetStartSample + lrint(proportion * (targetEndSample - targetStartSample)));
            if (target <= targets[targets.size()-1] + m_increment) {
                // peaks will become too close together afterwards, ignore
                ++peakidx;
                continue;
            }
            if (m_debugLevel > 1) {std::cerr << "  peak chunk " << pchunk << " (frame " << pchunk * m_increment << ") -> " << target << std::endl;}
            peaks.push_back(p);
            targets.push_back(target);
            ++peakidx;
        }
    }
}    
int
StretchCalculator::calculateSingle(double ratio, float df, size_t increment){
    if (increment == 0) increment = m_increment;
    auto isTransient = false;
    // We want to ensure, as close as possible, that the phase reset
    // points appear at _exactly_ the right audio frame numbers.
    // In principle, the threshold depends on chunk size: larger chunk
    // sizes need higher thresholds.  Since chunk size depends on
    // ratio, I suppose we could in theory calculate the threshold
    // from the ratio directly.  For the moment we're happy if it
    // works well in common situations.
    const auto transientThreshold = 0.35f;
//    if (ratio > 1) transientThreshold = 0.25f;
    if (m_useHardPeaks && df > m_prevDf * 1.1f && df > transientThreshold) {isTransient = true;}
    if (m_debugLevel > 2) {
        std::cerr << "df = " << df << ", prevDf = " << m_prevDf
                  << ", thresh = " << transientThreshold << std::endl;
    }
    m_prevDf = df;
    auto ratioChanged = (ratio != m_prevRatio);
    m_prevRatio = ratio;
    if (isTransient && m_transientAmnesty == 0) {
        if (m_debugLevel > 1) {
            std::cerr << "StretchCalculator::calculateSingle: transient (df " << df << ", threshold " << transientThreshold << ")" << std::endl;
        }
        m_divergence += increment - (increment * ratio);
        // as in offline mode, 0.05 sec approx min between transients
        m_transientAmnesty = lrint(ceil(double(m_sampleRate) / (20 * double(increment))));
        m_recovery = m_divergence / ((m_sampleRate / 10.0) / increment);
        return -int(increment);
    }
    if (ratioChanged) {m_recovery = m_divergence / ((m_sampleRate / 10.0) / increment);}
    if (m_transientAmnesty > 0) --m_transientAmnesty;
    auto incr = static_cast<int>(lrint(increment * ratio - m_recovery));
    if (m_debugLevel > 2 || (m_debugLevel > 1 && m_divergence != 0)) {
        std::cerr << "divergence = " << m_divergence << ", recovery = " << m_recovery << ", incr = " << incr << ", ";
    }
    if (incr < lrint((increment * ratio) / 2)) {incr = lrint((increment * ratio) / 2);}
    else if (incr > lrint(increment * ratio * 2)) {incr = lrint(increment * ratio * 2);}
    auto divdiff = (increment * ratio) - incr;
    if (m_debugLevel > 2 || (m_debugLevel > 1 && m_divergence != 0)) {std::cerr << "divdiff = " << divdiff << std::endl;}
    auto prevDivergence = m_divergence;
    m_divergence -= divdiff;
    if ((prevDivergence < 0 && m_divergence > 0) ||
        (prevDivergence > 0 && m_divergence < 0)) {
        m_recovery = m_divergence / ((m_sampleRate / 10.0) / increment);
    }
    return incr;
}
void
StretchCalculator::reset(){
    m_prevDf = 0;
    m_divergence = 0;
}
std::vector<StretchCalculator::Peak>
StretchCalculator::findPeaks(const std::vector<float> &rawDf){
    auto df = smoothDF(rawDf);
    // We distinguish between "soft" and "hard" peaks.  A soft peak is
    // simply the result of peak-picking on the smoothed onset
    // detection function, and it represents any (strong-ish) onset.
    // We aim to ensure always that soft peaks are placed at the
    // correct position in time.  A hard peak is where there is a very
    // rapid rise in detection function, and it presumably represents
    // a more broadband, noisy transient.  For these we perform a
    // phase reset (if in the appropriate mode), and we locate the
    // reset at the first point where we notice enough of a rapid
    // rise, rather than necessarily at the peak itself, in order to
    // preserve the shape of the transient.
    auto hardPeakCandidates = std::set<size_t>{}, softPeakCandidates = std::set<size_t>{};
    if (m_useHardPeaks) {
        // 0.05 sec approx min between hard peaks
        auto hardPeakAmnesty = static_cast<size_t>(lrint(ceil(double(m_sampleRate) /(20 * double(m_increment)))));
        auto prevHardPeak = size_t{0};
        if (m_debugLevel > 1) {std::cerr << "hardPeakAmnesty = " << hardPeakAmnesty << std::endl;}
        for (size_t i = 1; i + 1 < df.size(); ++i) {
            if (df[i] < 0.1f) continue;
            if (df[i] <= df[i-1] * 1.1f) continue;
            if (df[i] < 0.22f) continue;
            if (!hardPeakCandidates.empty() && i < prevHardPeak + hardPeakAmnesty) {
                continue;
            }
            auto hard = (df[i] > 0.4f);
            if (hard && (m_debugLevel > 1)) {
                std::cerr << "hard peak at " << i << ": " << df[i]  << " > absolute " << 0.4 << std::endl;
            }
            if (!hard) {
                hard = (df[i] > df[i-1] * 1.4f);
                if (hard && (m_debugLevel > 1)) {
                    std::cerr << "hard peak at " << i << ": " << df[i]  << " > prev " << df[i-1] << " * 1.4" << std::endl;
                }
            }
            if (!hard && i > 1) {
                hard = (df[i]   > df[i-1] * 1.2f && df[i-1] > df[i-2] * 1.2f);
                if (hard && (m_debugLevel > 1)) {
                    std::cerr << "hard peak at " << i << ": " << df[i] 
                              << " > prev " << df[i-1] << " * 1.2 and "
                              << df[i-1] << " > prev " << df[i-2] << " * 1.2"
                              << std::endl;
                }
            }
            if (!hard && i > 2) {
                // have already established that df[i] > df[i-1] * 1.1
                hard = (df[i] > 0.3f && df[i-1] > df[i-2] * 1.1f && df[i-2] > df[i-3] * 1.1f);
                if (hard && (m_debugLevel > 1)) {
                    std::cerr << "hard peak at " << i << ": " << df[i] 
                              << " > prev " << df[i-1] << " * 1.1 and "
                              << df[i-1] << " > prev " << df[i-2] << " * 1.1 and "
                              << df[i-2] << " > prev " << df[i-3] << " * 1.1"
                              << std::endl;
                }
            }
            if (!hard) continue;
//            (df[i+1] > df[i] && df[i+1] > df[i-1] * 1.8) ||
//                df[i] > 0.4) {
            auto peakLocation = i;
            if (i + 1 < rawDf.size() && rawDf[i + 1] > rawDf[i] * 1.4f) {
                ++peakLocation;
                if (m_debugLevel > 1) {
                    std::cerr << "pushing hard peak forward to " << peakLocation << ": " << df[peakLocation] << " > " << df[peakLocation-1] << " * " << 1.4 << std::endl;
                }
            }
            hardPeakCandidates.insert(peakLocation);
            prevHardPeak = peakLocation;
        }
    }
    auto medianmaxsize = static_cast<size_t>(lrint(ceil(double(m_sampleRate) /double(m_increment)))); // 1 sec ish
    if (m_debugLevel > 1) {std::cerr << "mediansize = " << medianmaxsize << std::endl;}
    if (medianmaxsize < 7) {
        medianmaxsize = 7;
        if (m_debugLevel > 1) { std::cerr << "adjusted mediansize = " << medianmaxsize << std::endl; }
    }
    auto minspacing = static_cast<int>(lrint(ceil(double(m_sampleRate) / (20 * double(m_increment))))); // 0.05 sec ish
    std::deque<float>  medianwin;
    std::vector<float> sorted;
    auto softPeakAmnesty = 0;
    std::fill_n(std::back_inserter(medianwin), medianmaxsize/2, 0.f);
    std::copy_n(df.begin(),std::min(medianmaxsize/2,df.size()),std::back_inserter(medianwin));
    auto lastSoftPeak = size_t{0};
    for (size_t i = 0; i < df.size(); ++i) {
        auto  mediansize = medianmaxsize;
        if (medianwin.size() < mediansize) {mediansize = medianwin.size();}
        auto  middle = medianmaxsize / 2;
        if (middle >= mediansize) middle = mediansize-1;
        auto  nextDf = i + mediansize - middle;
        if (mediansize < 2) {
            if (mediansize > medianmaxsize) {medianwin.pop_front();}
            if (nextDf < df.size()) {medianwin.push_back(df[nextDf]);}
            else {medianwin.push_back(0);}
            continue;
        }
        sorted = std::vector<float>(medianwin.begin(),medianwin.begin()+mediansize);
        std::sort(sorted.begin(), sorted.end());
        auto n = .9; // percentile above which we pick peaks
        auto index = static_cast<size_t>((sorted.size() * n));
        if (index >= sorted.size()) index = sorted.size()-1;
        if (index == sorted.size()-1 && index > 0) --index;
        auto thresh = sorted[index];
//        if (m_debugLevel > 2) {
//            std::cerr << "medianwin[" << middle << "] = " << medianwin[middle] << ", thresh = " << thresh << std::endl;
//            if (medianwin[middle] == 0.f) {
//                std::cerr << "contents: ";
//                for (size_t j = 0; j < medianwin.size(); ++j) {
//                    std::cerr << medianwin[j] << " ";
//                }
//                std::cerr << std::endl;
//            }
//        }
        if (medianwin[middle] > thresh &&
            medianwin[middle] > medianwin[middle-1] &&
            medianwin[middle] > medianwin[middle+1] &&
            softPeakAmnesty == 0) {
            auto maxindex = middle;
            auto maxval   = medianwin[middle];
            for (size_t j = middle+1; j < mediansize && medianwin[j] >= medianwin[middle]; ++j) {
                if (medianwin[j] > maxval) {
                    maxval = medianwin[j];
                    maxindex = j;
                }
            }
            auto peak = i + maxindex - middle;
//            std::cerr << "i = " << i << ", maxindex = " << maxindex << ", middle = " << middle << ", so peak at " << peak << std::endl;
            if (softPeakCandidates.empty() || lastSoftPeak != peak) {
                if (m_debugLevel > 1) {
                    std::cerr << "soft peak at " << peak << " ("
                              << peak * m_increment << "): "
                              << medianwin[middle] << " > "
                              << thresh << " and "
                              << medianwin[middle]
                              << " > " << medianwin[middle-1] << " and "
                              << medianwin[middle]
                              << " > " << medianwin[middle+1]
                              << std::endl;
                }
                if (peak >= df.size()) {if (m_debugLevel > 2) {std::cerr << "peak is beyond end"  << std::endl;}} else {
                    softPeakCandidates.insert(peak);
                    lastSoftPeak = peak;
                }
            }
            softPeakAmnesty = minspacing + maxindex - middle;
            if (m_debugLevel > 2) {std::cerr << "amnesty = " << softPeakAmnesty << std::endl;}
        } else if (softPeakAmnesty > 0) --softPeakAmnesty;
        if (mediansize >= medianmaxsize) {medianwin.pop_front();}
        if (nextDf < df.size()) {medianwin.push_back(df[nextDf]);
        } else {medianwin.push_back(0);}
    }
    auto peaks = std::vector<Peak>{};
    while (!hardPeakCandidates.empty() || !softPeakCandidates.empty()) {
        auto haveHardPeak = !hardPeakCandidates.empty();
        auto haveSoftPeak = !softPeakCandidates.empty();
        auto hardPeak = (haveHardPeak ? *hardPeakCandidates.begin() : size_t{0});
        auto softPeak = (haveSoftPeak ? *softPeakCandidates.begin() : size_t{0});
        auto peak = Peak{softPeak,false};
        auto ignore = false;
        if (haveHardPeak &&
            (!haveSoftPeak || hardPeak <= softPeak)) {
            if (m_debugLevel > 2) {std::cerr << "Hard peak: " << hardPeak << std::endl;}
            peak.hard = true;
            peak.chunk = hardPeak;
            hardPeakCandidates.erase(hardPeakCandidates.begin());
        } else {
            if (m_debugLevel > 2) {std::cerr << "Soft peak: " << softPeak << std::endl;}
            if (!peaks.empty() &&
                peaks[peaks.size()-1].hard &&
                peaks[peaks.size()-1].chunk + 3 >= softPeak) {
                if (m_debugLevel > 2) {
                    std::cerr << "(ignoring, as we just had a hard peak)"<< std::endl;
                }
                ignore = true;
            }
        }            
        if (haveSoftPeak && peak.chunk == softPeak) 
        {softPeakCandidates.erase(softPeakCandidates.begin());}
        if (!ignore) {peaks.push_back(peak);}
    }                
    return peaks;
}
std::vector<float>
StretchCalculator::smoothDF(const std::vector<float> &df){
    auto smoothedDF = std::vector<float>{};
    for (size_t i = 0; i < df.size(); ++i) {
        // three-value moving mean window for simple smoothing
        auto total = 0.f, count = 0.f;
        if (i > 0) { total += df[i-1]; ++count; }
        total += df[i]; ++count;
        if (i+1 < df.size()) { total += df[i+1]; ++count; }
        auto mean = total / count;
        smoothedDF.push_back(mean);
    }
    return smoothedDF;
}
std::vector<int>
StretchCalculator::distributeRegion(const std::vector<float> &dfIn,size_t duration, float ratio, bool phaseReset){
    auto df = dfIn;
    auto increments = std::vector<int>{};
    // The peak for the stretch detection function may appear after
    // the peak that we're using to calculate the start of the region.
    // We don't want that.  If we find a peak in the first half of
    // the region, we should set all the values up to that point to
    // the same value as the peak.

    // (This might not be subtle enough, especially if the region is
    // long -- we want a bound that corresponds to acoustic perception
    // of the audible bounce.)

    for (size_t i = 1; i < df.size()/2; ++i) {
        if (df[i] < df[i-1]) {
            if (m_debugLevel > 1) {
                std::cerr << "stretch peak offset: " << i-1 << " (peak " << df[i-1] << ")" << std::endl;
            }
            for (size_t j = 0; j < i-1; ++j) {df[j] = df[i-1];}
            break;
        }
    }
    auto maxDf = 0.f;
    for (size_t i = 0; i < df.size(); ++i) {if (i == 0 || df[i] > maxDf) maxDf = df[i];}
    // We want to try to ensure the last 100ms or so (if possible) are
    // tending back towards the maximum df, so that the stretchiness
    // reduces at the end of the stretched region.
    
    auto reducedRegion = static_cast<int>(lrint((0.1 * m_sampleRate) / m_increment));
    if (reducedRegion > int(df.size()/5)) reducedRegion = df.size()/5;
    for (auto i = 0; i < reducedRegion; ++i) {
        auto index = df.size() - reducedRegion + i;
        df[index] = df[index] + ((maxDf - df[index]) * i) / reducedRegion;
    }
    auto toAllot = long(duration) - long(m_increment * df.size());
    if (m_debugLevel > 1) {
        std::cerr << "region of " << df.size() << " chunks, output duration " << duration << ", increment " << m_increment << ", toAllot " << toAllot << std::endl;
    }
    auto totalIncrement = size_t{0};
    // We place limits on the amount of displacement per chunk.  if
    // ratio < 0, no increment should be larger than increment*ratio
    // or smaller than increment*ratio/2; if ratio > 0, none should be
    // smaller than increment*ratio or larger than increment*ratio*2.
    // We need to enforce this in the assignment of displacements to
    // allotments, not by trying to respond if something turns out
    // wrong.

    // Note that the ratio is only provided to this function for the
    // purposes of establishing this bound to the displacement.
    
    // so if
    // maxDisplacement / totalDisplacement > increment * ratio*2 - increment
    // (for ratio > 1)
    // or
    // maxDisplacement / totalDisplacement < increment * ratio/2
    // (for ratio < 1)
    // then we need to adjust and accommodate
    auto  totalDisplacement = 0.0;
    auto  maxDisplacement = 0.0; // min displacement will be 0 by definition
    maxDf = 0;
    auto adj = 0.f;
    auto tooShort = true, tooLong = true;
    const auto acceptableIterations = 10;
    auto iteration = 0;
    auto  prevExtreme = 0;
    auto better = false;
    while ((tooLong || tooShort) && iteration < acceptableIterations) {
        ++iteration;
        tooLong = false;
        tooShort = false;
        calculateDisplacements(df, maxDf, totalDisplacement, maxDisplacement,adj);
        if (m_debugLevel > 1) {
            std::cerr << "totalDisplacement " << totalDisplacement << ", max " << maxDisplacement << " (maxDf " << maxDf << ", df count " << df.size() << ")" << std::endl;
        }
        if (totalDisplacement == 0) {
// Not usually a problem, in fact
//            std::cerr << "WARNING: totalDisplacement == 0 (duration " << duration << ", " << df.size() << " values in df)" << std::endl;
            if (!df.empty() && adj == 0) {
                tooLong = true; tooShort = true;
                adj = 1;
            }
            continue;
        }
        auto extremeIncrement = static_cast<int>(m_increment +lrint((toAllot * maxDisplacement) / totalDisplacement));
        if (extremeIncrement < 0) {
            if (m_debugLevel > 0) {
                std::cerr << "NOTE: extreme increment " << extremeIncrement << " < 0, adjusting" << std::endl;
            }
            tooShort = true;
        } else {
            if (ratio < 1.0) {
                if (extremeIncrement > lrint(ceil(m_increment * ratio))) {
                    std::cerr << "WARNING: extreme increment "
                              << extremeIncrement << " > "
                              << m_increment * ratio << std::endl;
                } else if (extremeIncrement < (m_increment * ratio) / 2) {
                    if (m_debugLevel > 0) {
                        std::cerr << "NOTE: extreme increment "
                                  << extremeIncrement << " < " 
                                  << (m_increment * ratio) / 2
                                  << ", adjusting" << std::endl;
                    }
                    tooShort = true;
                    if (iteration > 0) {better = (extremeIncrement > prevExtreme);}
                    prevExtreme = extremeIncrement;
                }
            } else {
                if (extremeIncrement > m_increment * ratio * 2) {
                    if (m_debugLevel > 0) {
                        std::cerr << "NOTE: extreme increment "
                                  << extremeIncrement << " > "
                                  << m_increment * ratio * 2
                                  << ", adjusting" << std::endl;
                    }
                    tooLong = true;
                    if (iteration > 0) {better = (extremeIncrement < prevExtreme);}
                    prevExtreme = extremeIncrement;
                } else if (extremeIncrement < lrint(floor(m_increment * ratio))) {
                    std::cerr << "WARNING: extreme increment "
                              << extremeIncrement << " < "
                              << m_increment * ratio << std::endl;
                }
            }
        }
        if (tooLong || tooShort) {
            // Need to make maxDisplacement smaller as a proportion of
            // the total displacement, yet ensure that the
            // displacements still sum to the total.
            adj += maxDf/10;
        }
    }
    if (tooLong) {
        if (better) {
            // we were iterating in the right direction, so
            // leave things as they are (and undo that last tweak)
            std::cerr << "WARNING: No acceptable displacement adjustment found, using latest values:\nthis region could sound bad" << std::endl;
            adj -= maxDf/10;
        } else {
            std::cerr << "WARNING: No acceptable displacement adjustment found, using defaults:\nthis region could sound bad" << std::endl;
            adj = 1;
            calculateDisplacements(df, maxDf, totalDisplacement, maxDisplacement,adj);
        }
    } else if (tooShort) {
        std::cerr << "WARNING: No acceptable displacement adjustment found, using flat distribution:\nthis region could sound bad" << std::endl;
        adj = 1;
        for (size_t i = 0; i < df.size(); ++i) {df[i] = 1.f;}
        calculateDisplacements(df, maxDf, totalDisplacement, maxDisplacement,adj);
    }
    for (size_t i = 0; i < df.size(); ++i) {
        auto displacement = maxDf - df[i];
        if (displacement < 0) displacement -= adj;
        else displacement += adj;
        if (i == 0 && phaseReset) {
            if (m_debugLevel > 2) {std::cerr << "Phase reset at first chunk" << std::endl;}
            if (df.size() == 1) {
                increments.push_back(duration);
                totalIncrement += duration;
            } else {
                increments.push_back(m_increment);
                totalIncrement += m_increment;
            }
            totalDisplacement -= displacement;
            continue;
        }
        auto theoreticalAllotment = 0.0;
        if (totalDisplacement != 0) {theoreticalAllotment = (toAllot * displacement) / totalDisplacement;}
        auto allotment = static_cast<int>(lrint(theoreticalAllotment));
        if (i + 1 == df.size()) allotment = toAllot;
        auto increment = static_cast<int>(m_increment + allotment);
        if (increment < 0) {
            // this is a serious problem, the allocation is quite
            // wrong if it allows increment to diverge so far from the
            // input increment (though it can happen legitimately if
            // asked to squash very violently)
            std::cerr << "*** WARNING: increment " << increment << " <= 0, rounding to zero" << std::endl;
            toAllot += m_increment;
            increment = 0;
        } else {toAllot -= allotment;}
        increments.push_back(increment);
        totalIncrement += increment;
        totalDisplacement -= displacement;
    }
    return increments;
}
void
StretchCalculator::calculateDisplacements(const std::vector<float> &df,
                                          float &maxDf,
                                          double &totalDisplacement,
                                          double &maxDisplacement,
                                          float adj) const{
    totalDisplacement = maxDisplacement = 0;
    if(UNLIKELY(!df.size())) return;
    maxDf=df[0];
    for (size_t i = 1; i < df.size(); ++i) {maxDf = (maxDf>df[i])?maxDf:df[i];}
    maxDisplacement = maxDf - df[0] + adj;
    totalDisplacement = maxDisplacement;
    for (size_t i = 1; i < df.size(); ++i) {
        auto displacement = maxDf - df[i] + adj;
        totalDisplacement += displacement;
        maxDisplacement = (maxDisplacement<displacement)?displacement:maxDisplacement;
    }
}
}

