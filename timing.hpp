#ifndef TIMING_HPP_INCLUDED
#define TIMING_HPP_INCLUDED

#include <sys/time.h>

timeval startTiming();
float getElapsedSec(timeval start);

#endif // TIMING_HPP_INCLUDED
