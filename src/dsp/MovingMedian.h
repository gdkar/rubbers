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

#ifndef _MOVING_MEDIAN_H_
#define _MOVING_MEDIAN_H_

#include "SampleFilter.h"
#include <memory>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace RubberBand
{

template <typename T>
class MovingMedian : public SampleFilter<T>{
    typedef SampleFilter<T> P;
    using SampleFilter<T>::m_size;
    std::unique_ptr<T[]> m_frame        = nullptr;
    std::unique_ptr<T[]> m_sorted       = nullptr;
    T *                  m_sorted_end   = nullptr;
    size_t      m_index                    = 0;
    size_t      m_head                     = 0;
    void put(T value) {
	// precondition: m_sorted contains m_size-1 values, packed at start
	// postcondition: m_sorted contains m_size values, one of which is value
	auto index = static_cast<T*>(std::lower_bound(&m_sorted[0], m_sorted_end, value));
        std::memmove(index + 1, index, (reinterpret_cast<char*>(m_sorted_end) - reinterpret_cast<char*>(index)));
	*index = value;
    }
    void drop(T value) {
	// precondition: m_sorted contains m_size values, one of which is value
	// postcondition: m_sorted contains m_size-1 values, packed at start
        auto index = static_cast<T*>(std::lower_bound(&m_sorted[0],m_sorted_end+1,value));
	assert(*index == value);
        std::memmove(index,index+1,reinterpret_cast<char*>(m_sorted_end) - reinterpret_cast<char*>(index));
	*m_sorted_end = T(0);
    }
public:
    MovingMedian(int size, float percentile = 0.5f) :
        SampleFilter<T>(size),
	m_frame(std::make_unique<T[]>(size)),
	m_sorted(std::make_unique<T[]>(size)),
	m_sorted_end(&m_sorted[0] + m_size - 1) {
        setPercentile(percentile);
        reset();
    }
    virtual ~MovingMedian() { }
    void setPercentile(float p) {
        m_index = static_cast<size_t>((m_size * p));
        if (m_index >= m_size) m_index = m_size-1;
        if (m_index < 0) m_index = 0;
    }
    virtual void push(T value) {
        if (value != value) {
            std::cerr << "WARNING: MovingMedian: NaN encountered" << std::endl;
            value = T();
        }
        drop(m_frame[m_head]);
        m_frame[m_head] = value;
	put (m_frame[m_head]);
        m_head ++;
        m_head *= (m_head < m_size);
    }
    virtual T get() const {return m_sorted[m_index];}
    virtual void reset() {
        std::fill ( &m_frame[0],  &m_frame[m_size],  0);
        std::fill ( &m_sorted[0], &m_sorted[m_size], 0);
        m_head = 0;
    }
};
}
#endif
