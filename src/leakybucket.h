// Copyright (c) 2015 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LEAKYBUCKET_H
#define BITCOIN_LEAKYBUCKET_H

#include <boost/chrono/include.hpp>

/** Default value for the maximum amount of data that can be received in a burst */
static const int DEFAULT_MAX_RECV_BURST = 100 * 1024;
/** Default value for the maximum amount of data that can be sent in a burst */
static const int DEFAULT_MAX_SEND_BURST = 100 * 1024;
/** Default value for the average amount of data received per second */
static const int DEFAULT_AVE_RECV = 30 * 1024;
/** Default value for the average amount of data sent per second */
static const int DEFAULT_AVE_SEND = 10 * 1024;
/** If we have to break the transmission up into chunks, this is the minimum send chunk size */
static const int SEND_SHAPER_MIN_FRAG = 256;
/** If we have to break the transmission up into chunks, this is the minimum receive chunk size */
static const int RECV_SHAPER_MIN_FRAG = 256;

class CLeakyBucket
{
public:
    CLeakyBucket(int maxp, int fillp, int startLevel = INT_MAX);

    // Stop the operation of this leaky bucket (always return available tokens)
    // use "set" to restart
    void disable();

    // Access the values in this bucket
    void get(int* maximum, int* fillAmount, int *current = NULL);

    // Change the settings of the leaky bucket
    void set(int maximum, int fillAmount);

    // Return the # tokens available if that amount is larger than the cutoff, otherwise return 0
    int available(int cutoff = 0);

    // Try to use \a amount tokens.  Returns true if the tokens can be consumed, false otherwise
    bool try_consume(int amount);

    // This function reduces the level in the bucket by amt, even if that makes the
    // level negative, and returns true if the level is >= 0. This function is useful
    // in a situation like data receipt (with soft limits) where you are not certain
    // how many bytes will be received until after you have received them.
    bool consume(int amount);

protected:
    typedef boost::chrono::steady_clock CClock;

    int level; // Current level of the bucket
    int max; // Maximum quantity allowed
    int fill; // Average rate per second
    static CClock clock;
    boost::chrono::time_point<CClock> lastFill;

    // This function is called internally to fill the leaky bucket based on
    // the time difference between now and the last time the function was called.
    void fillIt();
};

#endif
