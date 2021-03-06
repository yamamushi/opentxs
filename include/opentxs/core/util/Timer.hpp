//////////////////////////////////////////////////////////////////////////////
// Timer.hpp
// =======
// High Resolution Timer.
// This timer is able to measure the elapsed time with 1 micro-second accuracy
// in both Windows, Linux and Unix system
//
//  AUTHOR: Song Ho Ahn (song.ahn@gmail.com)
// CREATED: 2003-01-13
// UPDATED: 2006-01-13
//
// Copyright (c) 2003 Song Ho Ahn
//////////////////////////////////////////////////////////////////////////////

#ifndef OPENTXS_CORE_TIMER_HPP
#define OPENTXS_CORE_TIMER_HPP

#include "opentxs/Forward.hpp"

#include <cinttypes>
#include <cstdint>

#ifdef _WIN32
#ifndef _WINDOWS_
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#else
#include <sys/time.h>
#endif

class Timer
{
public:
    EXPORT Timer(bool bStart = false);  // default constructor // FT: added
                                        // default argument.
    EXPORT ~Timer();                    // default destructor

    EXPORT void start();  // start timer
    void stop();          // stop the timer
    void clear();         // stop the timer and clear the contents.

    double getElapsedTime();       // get elapsed time in second
    double getElapsedTimeInSec();  // get elapsed time in second (same as
                                   // getElapsedTime)
    EXPORT double getElapsedTimeInMilliSec();  // get elapsed time in
                                               // milli-second
    double getElapsedTimeInMicroSec();  // get elapsed time in micro-second

protected:
private:
    double startTimeInMicroSec;  // starting time in micro-second
    double endTimeInMicroSec;    // ending time in micro-second
    std::int32_t stopped;        // stop flag
#ifdef WIN32
    LARGE_INTEGER frequency;   // ticks per second
    LARGE_INTEGER startCount;  //
    LARGE_INTEGER endCount;    //
#else
    timeval startCount;  //
    timeval endCount;    //
#endif
};

#endif  // OPENTXS_CORE_TIMER_HPP
