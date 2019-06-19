// Copyright (c) 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_CARRYFORWARD_H
#define SAFIRE_CARRYFORWARD_H

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <ctime>

#include <stdio.h>
#include <thread>
#include <unistd.h>

class CCarryforward
{
private:
    
public:
    //! Construct
    CCarryforward()
    {
    }
    
    //! Destructor
    ~CCarryforward()
    {
    }
    
    void carryforwardThread(int argc, char* argv[]);
    void stop();
    
};

#endif // SAFIRE_CARRYFORWARD_H






