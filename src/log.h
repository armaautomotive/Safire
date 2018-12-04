// Copyright (c) 2018 Jon Taylor 
// Website: http://safire.org
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_LOG_H
#define SAFIRE_LOG_H

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
//#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

class CFileLogger
{
private:

public:
    //! Construct an invalid private key.
    CFileLogger()
    {
    }

    //! Destructor (again necessary because of memlocking).
    ~CFileLogger()
    {
    }

    void clearLog();
    void log(std::string str);
};

#endif // SAFIRE_LOG_H

