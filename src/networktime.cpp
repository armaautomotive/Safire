// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "networktime.h"
#include <time.h>

long CNetworkTime::networkTimeOffset = 0;

long CNetworkTime::getEpoch(){
    return getLocalEpoch() + networkTimeOffset;
}

long CNetworkTime::getLocalEpoch(){
    time_t timev;
    time(&timev);
    return static_cast<long>(timev);
}

long CNetworkTime::getOffset(){
    return networkTimeOffset;
}

void CNetworkTime::setOffset(long offset){
    networkTimeOffset = offset;
}

bool CNetworkTime::isClockHealthy(){
    return networkTimeOffset > -30 && networkTimeOffset < 30;
}
