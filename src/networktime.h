#ifndef MAGNITE_TIME_H
#define MAGNITE_TIME_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>


class CTime
{
private:
    //! Whether this private key is valid. We check for correctness when modifying the key
    //! data, so fValid should always correspond to the actual state.
    bool fValid;


public:
    //! Construct an invalid private key.
    CTime()
    {
    }

    //! Destructor (again necessary because of memlocking).
    ~CTime()
    {
    }


};

#endif // MAGNITE_TIME_H
