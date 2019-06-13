// Copyright (c) 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_HEARTBEAT_H
#define SAFIRE_HEARTBEAT_H

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

class CHeartbeat
{
private:
    
public:
    //! Construct
    CHeartbeat()
    {
    }
    
    //! Destructor
    ~CHeartbeat()
    {
    }
    
    void heartbeatThread(int argc, char* argv[]);
    void stop();
    
};

#endif // SAFIRE_HEARTBEAT_H



