#ifndef MAGNITE_NETWORK_TIME_H
#define MAGNITE_NETWORK_TIME_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>


class CNetworkTime
{
private:
    //! Whether this private key is valid. We check for correctness when modifying the key
    //! data, so fValid should always correspond to the actual state.
    bool fValid;


public:
    //! Construct an invalid private key.
    CNetworkTime()
    {
    }

    //! Destructor (again necessary because of memlocking).
    ~CNetworkTime()
    {
    }

    long getEpoch();
};

#endif // MAGNITE_NETWORK_TIME_H
