// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAGNITE_BLOCK_DB_H
#define MAGNITE_BLOCK_DB_H

#include "functions/functions.h"
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

class CBlockDB
{
private:


public:
    //! Construct an invalid private key.
    CBlockDB()
    {
    }

    //! Destructor (again necessary because of memlocking).
    ~CBlockDB()
    {
    }

    //int AddBlock(std::string publicKey);
    bool addFirstBlock(CFunctions::block_structure block);
    bool AddBlock(CFunctions::block_structure block);
    void GetBlocks();
    CFunctions::block_structure getFirstBlock();
    CFunctions::block_structure getBlock(long number);
    CFunctions::block_structure GetBlockWithSender( std::string sender_key, int index );
};

#endif // SAFIRE_BLOCK_DB_H
