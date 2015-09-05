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

#ifndef _RUBBERBAND_SCAVENGER_H_
#define _RUBBERBAND_SCAVENGER_H_

#include <vector>
#include <list>
#include <utility>
#include <iostream>
#include <chrono>

#include "system/Thread.h"
#include "system/sysutils.h"
#include "system/Allocators.h"

//#define DEBUG_SCAVENGER 1

namespace Rubbers {

/**
 * A very simple class that facilitates running things like plugins
 * without locking, by collecting unwanted objects and deleting them
 * after a delay so as to be sure nobody's in the middle of using
 * them.  Requires scavenge() to be called regularly from a non-RT
 * thread.
 *
 * This is currently not at all suitable for large numbers of objects
 * -- it's just a quick hack for use with things like plugins.
 */

//!!! should review this, it's not really thread safe owing to lack of
//!!! atomic updates

template <typename T>
class Scavenger
{
public:
    Scavenger(int sec = 2, int defaultObjectListSize = 200);
    virtual ~Scavenger();
    /**
     * Call from an RT thread etc., to pass ownership of t to us.
     * Only one thread should be calling this on any given scavenger.
     */
    void claim(T *t);
    /**
     * Call from a non-RT thread.
     * Only one thread should be calling this on any given scavenger.
     */
    void scavenge(bool clearNow = false);
protected:
    typedef std::pair<T *, int> ObjectTimePair;
    typedef std::vector<ObjectTimePair> ObjectTimeList;
    ObjectTimeList m_objects;
    int m_sec;
    typedef std::list<T *> ObjectList;
    ObjectList m_excess;
    int m_lastExcess;
    std::mutex m_excessMutex;
    void pushExcess(T *);
    void clearExcess(int);
    unsigned int m_claimed;
    unsigned int m_scavenged;
    unsigned int m_asExcess;
};
/**
 * A wrapper to permit arrays allocated with new[] to be scavenged.
 */
template <typename T>
class ScavengerArrayWrapper
{
public:
    ScavengerArrayWrapper(T *array) : m_array(array) { }
    virtual ~ScavengerArrayWrapper() { delete[] m_array; }
private:
    T *m_array;
};
/**
 * A wrapper to permit arrays allocated with the Allocators functions
 * to be scavenged.
 */

template <typename T>
class ScavengerAllocArrayWrapper
{
public:
    ScavengerAllocArrayWrapper(T *array) : m_array(array) { }
    virtual ~ScavengerAllocArrayWrapper() { deallocate<T>(m_array); }
private:
    T *m_array;
};
template <typename T>
Scavenger<T>::Scavenger(int sec, int defaultObjectListSize) :
    m_objects(ObjectTimeList(defaultObjectListSize)),
    m_sec(sec),
    m_lastExcess(0),
    m_claimed(0),
    m_scavenged(0),
    m_asExcess(0)
{}
template <typename T>
Scavenger<T>::~Scavenger(){
    if (m_scavenged < m_claimed) {
        for ( auto &pair : m_objects )
        {
            if ( pair.first ) 
            {
                auto ot = std::exchange ( pair.first, nullptr );
                delete[] ot;
                ++ m_scavenged;
            }
        }
    }
    clearExcess(0);
}
template <typename T>
void
Scavenger<T>::claim(T *t){
//    std::cerr << "Scavenger::claim(" << t << ")" << std::endl;
    auto tv = std::chrono::high_resolution_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::nanoseconds>(tv.time_since_epoch()).count();
    for ( auto &pair : m_objects )
    {
        if ( pair.first == 0 )
        {
            pair.second = sec;
            pair.first  = t;
            ++ m_claimed;
            return;
        }
    }
    pushExcess(t);
}
template <typename T>
void
Scavenger<T>::scavenge(bool clearNow){
#ifdef DEBUG_SCAVENGER
    std::cerr << "Scavenger::scavenge: claimed " << m_claimed << ", scavenged " << m_scavenged << ", cleared as excess " << m_asExcess << std::endl;
#endif
    if (m_scavenged >= m_claimed) return;
    auto tv = std::chrono::high_resolution_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::nanoseconds>(tv.time_since_epoch()).count();
    auto anything = false;
    for ( auto &pair : m_objects ){
        if (!pair.first) continue;
	if (clearNow || pair.second + m_sec < sec) 
        {
	    auto ot = std::exchange(pair.first,nullptr);
	    delete ot;
	    ++m_scavenged;
            anything = true;
	}
    }
    if (clearNow || anything || (sec > m_lastExcess + m_sec)) {clearExcess(sec);}
}
template <typename T>
void
Scavenger<T>::pushExcess(T *t){
    std::unique_lock<std::mutex> lock(m_excessMutex);
    m_excess.push_back(t);
    auto sec = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    m_lastExcess = sec;
}
template <typename T>
void
Scavenger<T>::clearExcess(int sec){
#ifdef DEBUG_SCAVENGER
    std::cerr << "Scavenger::clearExcess: Excess now " << m_excess.size() << std::endl;
#endif
    std::unique_lock<std::mutex> lock(m_excessMutex);
    for ( auto it : m_excess )
    {
        delete it;
        ++ m_asExcess;
    }
    m_excess.clear();
    m_lastExcess = sec;
}
}
#endif
