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

#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>


namespace RubberBand
{

class Thread
{
    std::thread m_thread;
    static void static_run( Thread *self){self->run();}
protected:
    virtual void run() = 0;
    virtual void start(){m_thread = std::thread(&Thread::static_run,this);}
public:
    Thread(){}
    virtual ~Thread(){if(m_thread.joinable() && m_thread.get_id()!=std::this_thread::get_id()) m_thread.join();}
    std::thread::id id()  const{return m_thread.get_id();}
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

class Condition : public std::condition_variable {
    std::mutex  m_mutex;
public:
    explicit Condition() = default;
    virtual ~Condition() = default;
    void lock(){m_mutex.lock();}
    void unlock(){m_mutex.unlock();}
    void wait(){
        std::unique_lock<std::mutex> lock(m_mutex);
        std::condition_variable::wait(lock);
    }
    template<class Clock, class Period>
    void wait_for(std::chrono::duration<Clock,Period> d){
        std::unique_lock<std::mutex> lock(m_mutex);
        std::condition_variable::wait_for(lock,d);
    }
    template<class Clock, class Duration>
    void wait_until(std::chrono::time_point<Clock,Duration> tp){
        std::unique_lock<std::mutex> lock(m_mutex);
        std::condition_variable::wait_until(lock,tp);
    }
    void notify_all(){
        std::unique_lock<std::mutex> lock(m_mutex);
        std::condition_variable::notify_all();
    }
    void notify_any(){
        std::unique_lock<std::mutex> lock(m_mutex);
        std::condition_variable::notify_all();
    }
};

}
