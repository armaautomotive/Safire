// Copyright (c) 2016,2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_BLOCK_DB_H
#define SAFIRE_BLOCK_DB_H

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
#include "leveldb/db.h"

class CBlockDB
{
private:
    static leveldb::DB * db;

public:
    
    //! Construct an invalid private key.
    CBlockDB();

    //! Destructor (again necessary because of memlocking).
    ~CBlockDB();

    leveldb::DB * getDatabase();
    leveldb::DB * getDatabase2();
    
    //bool addFirstBlock(CFunctions::block_structure block);
    bool AddBlock(CFunctions::block_structure block);
    void setFirstBlockId(long number);
    long getFirstBlockId();
    void setLatestBlockId(long number);
    long getLatestBlockId();
    long getNextBlockId(long previousBlockId);
    CFunctions::block_structure getNextBlock(CFunctions::block_structure block);
    
    void GetBlocks();
    //CFunctions::block_structure getFirstBlock();
    CFunctions::block_structure getBlock(long number);
    CFunctions::block_structure GetBlockWithSender(std::string sender_key, int index);
    void DeleteAll();
    
    void setScannedBlockId(long number);
    long getScannedBlockId();
    
    void AddUser(CFunctions::user_structure user);
    
    
};

#endif // SAFIRE_BLOCK_DB_H
