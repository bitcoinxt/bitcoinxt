// Copyright (c) 2015 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "leakybucket.h"

CLeakyBucket::CLeakyBucket(int maxp, int fillp, int startLevel)
    : max(maxp),
    fill(fillp)
{
    lastFill = clock.now();
    // set the initial level to either what is specified by the user or to the maximum
    level = (startLevel < max) ? startLevel : max;
}

void CLeakyBucket::disable()
{
    fill = 0;
    max = 0;
}

void CLeakyBucket::fillIt()
{
    boost::chrono::time_point<CClock> now = clock.now();
    CClock::duration elapsed(now - lastFill);
    int msElapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(elapsed).count();
    // note in practice msElapsed can be < 0, something to do with hyperthreading
    // so don't eliminate this conditional
    if (msElapsed > 100)
    {
        lastFill = now;
        level += (fill * msElapsed) / 1000;
        if (level > max)
            level = max;
    }
}

void CLeakyBucket::get(int* maxp, int* fillp, int* levelp)
{
    if (maxp) *maxp = max;
    if (fillp) *fillp = fill;
    if (levelp) *levelp = level;
}

void CLeakyBucket::set(int maxp, int fillp)
{
    max = maxp;
    fill = fillp;
    if (level > max)
        level=max; // if pinching, slow traffic quickly.
    lastFill = clock.now(); // need to reset the lastFill time in case we are turning on this leaky bucket.
}

int CLeakyBucket::available(int cutoff)
{
    if (fill == 0)
        return INT_MAX; // shaping is off
    fillIt();
    return (level > cutoff) ? level : 0;
}

bool CLeakyBucket::try_consume(int amt)
{
    if (fill == 0)
        return true; // leaky bucket is turned off.
    assert(amt >= 0);
    fillIt();
    if (level >= amt)
    {
        level-=amt;
        return true;
    }
    return false;
}

bool CLeakyBucket::consume(int amt)
{
    if (fill == 0)
        return true; // leaky bucket is turned off.
    fillIt();
    level -= amt;
    return (level >= 0);
}
