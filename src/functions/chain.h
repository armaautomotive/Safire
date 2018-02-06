// Copyright (c) 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_CHAIN_H
#define SAFIRE_CHAIN_H

#include "functions/functions.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>


class CChain
{
private:
    long firstBlock;
    long latestBlock;
public:
    CChain();

    ~CChain()
    {
    }

    bool fileExists(std::string fileName);
    void writeFile();
    void loadFile();
    void setFirstBlock(CFunctions::block_structure block);
    long getFirstBlock();
    void setLatestBlock(CFunctions::block_structure block);
    long getLatestBlock();
};

#endif // SAFIRE_CHAIN_H
