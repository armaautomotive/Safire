#ifndef SAFIRE_HEX_H
#define SAFIRE_HEX_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

class CHex
{
private:

public:
    //! Construct an invalid private key.
    CHex()
    {
    }

    //! Destructor (again necessary because of memlocking).
    ~CHex()
    {
    }

    std::string to64(std::string & hex);
    //void printCommands();
    //void printAdvancedCommands();
};

#endif // SAFIRE_HEX_H
