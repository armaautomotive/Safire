// Copyright (c) 2018 Jon Taylor 
// Website: http://safire.org
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_CLI_H
#define SAFIRE_CLI_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

class CLOG
{
private:


public:
    //! Construct an invalid private key.
    CLOG()
    {
    }

    //! Destructor (again necessary because of memlocking).
    ~CLOG()
    {
    }

    void clearLog();
    void log(std::string str);
};

#endif // SAFIRE_LOG_H

