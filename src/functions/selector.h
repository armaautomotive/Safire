// Copyright (c) 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_SELECTOR_H
#define SAFIRE_SELECTOR_H

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>

class CSelector
{
private:
    int secondsPerBlock = 15;
    long networkTimeOffset = 0;
public:
    //! Construct
    CSelector()
    {
    }

    //! Destructor
    ~CSelector()
    {
    }

    void syncronizeTime();
    bool isSelected(std::string publicKey);
    std::string getSelectedUser(long time);
    long getCurrentTimeBlock();
    void addUser(std::string user);

    static std::vector< std::string > users;
};

#endif // SAFIRE_SELECTOR_H


