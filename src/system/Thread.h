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

#ifndef _RUBBERBAND_THREAD_H_
#define _RUBBERBAND_THREAD_H_

#include <string>

#include <thread>
#include <mutex>
#include <condition_variable>
#ifdef _WIN32
#include <windows.h>
#else /* !_WIN32 */
#ifdef USE_PTHREADS
#include <pthread.h>
#else /* !USE_PTHREADS */
#error No thread implementation selected
#endif /* !USE_PTHREADS */
#endif /* !_WIN32 */

//#define DEBUG_THREAD 1
//#define DEBUG_MUTEX 1
//#define DEBUG_CONDITION 1

namespace RubberBand
{

class Thread
{
    std::thread m_thread;
    static void static_run( Thread *self){self->run();}
protected:
    virtual void run() = 0;
public:
    Thread(){}
    virtual ~Thread(){wait();}
    std::thread::id id()  const{return m_thread.get_id();}
    virtual bool extant() const{return m_thread.joinable();}
    virtual void start(){
        m_thread = std::thread(&Thread::static_run,this);
    }
    virtual void wait(){
        if(extant() && id()!=std::this_thread::get_id()) 
            m_thread.join();
    }
    static constexpr bool threadingAvailable(){return true;}
};


/**
  The Condition class bundles a condition variable and mutex.

  To wait on a condition, call lock(), test the termination condition
  if desired, then wait().  The condition will be unlocked during the
  wait and re-locked when wait() returns (which will happen when the
  condition is signalled or the timer times out).

  To signal a condition, call signal().  If the condition is signalled
  between lock() and wait(), the signal may be missed by the waiting
  thread.  To avoid this, the signalling thread should also lock the
  condition before calling signal() and unlock it afterwards.
*/

class Condition
{
    std::mutex  m_mutex;
    std::condition_variable m_condition;
    bool m_locked;
    std::string m_name;
public:
    Condition(std::string name):m_locked(false),m_name(name){}
    ~Condition(){}
    inline void lock(){m_mutex.lock();m_locked = true;}
    inline void unlock(){m_locked=false;m_mutex.unlock();}
    inline void wait(int64_t ns = 0){
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait_for(lock,std::chrono::nanoseconds(ns));
    }
    inline void signal(){
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.notify_all();
    }
};

}
#endif
