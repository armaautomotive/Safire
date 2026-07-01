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
    static bool schemaMigrated;
    void migrateSchema(leveldb::DB* ldb);

public:
    static const int CURRENT_SCHEMA_VERSION = 2;

    struct fork_variant {
        long block_number;
        std::string hash;
        std::string previous_hash;
        std::string creator_key;
        bool canonical;
    };

    struct reorg_info {
        long previous_block;
        std::string previous_hash;
        long new_block;
        std::string new_hash;
        long time;
        std::string reason;
    };

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
    std::string getLatestBlockHash();
    int getSchemaVersion();
    long getConnectedLatestBlockId();
    long rebuildBestChainIndex();
    long getForkVariantCount();
    std::vector<fork_variant> getForkVariants(int limit);
    reorg_info getLastReorgInfo();
    long getNextBlockId(long previousBlockId);
    CFunctions::block_structure getNextBlockByHash(CFunctions::block_structure block);
    CFunctions::block_structure getNextBlock(CFunctions::block_structure block);
    
    void GetBlocks();
    std::vector<CFunctions::block_structure> getStoredBlocks();
    CFunctions::block_structure getBlockByHash(std::string hash);
    //CFunctions::block_structure getFirstBlock();
    CFunctions::block_structure getBlock(long number);
    CFunctions::block_structure GetBlockWithSender(std::string sender_key, int index);
    void DeleteAll();
    
    void setScannedBlockId(long number);
    long getScannedBlockId();
    
    void setUser(CFunctions::user_structure user);
    CFunctions::user_structure getUser(std::string public_key);
    void DeleteIndex();
    
};

#endif // SAFIRE_BLOCK_DB_H
