// Copyright (c) 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_PLATFORM_H
#define SAFIRE_PLATFORM_H

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>

class CPlatform
{
private:

public:
    CPlatform()
    {
    }

    ~CPlatform()
    {
    }

    std::string getSafirePath();
};

#endif // SAFIRE_PLATFORM_H
