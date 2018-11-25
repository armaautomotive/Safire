// Copyright (c) 2016 2017 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockdb.h"
#include <sstream>
#include <unistd.h>   // open and close
#include "leveldb/db.h"
#include "platform.h"
#include <boost/lexical_cast.hpp>

using namespace std;

/**
 * addFirstBlock
 *
 * Description: Add block entry. NOT USED.
 *
 */
bool CBlockDB::addFirstBlock(CFunctions::block_structure block){ 
    CPlatform platform;
    CFunctions functions;
    std::string dbPath = platform.getSafirePath();
    //std::cout << dbPath << std::endl;

    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './blockdb'" << endl;
        cerr << status.ToString() << endl;
        return -1;
    }

    leveldb::WriteOptions writeOptions;

    // Insert
    ostringstream keyStream;
    keyStream << "first_block"; // boost::lexical_cast<std::string>(block.number);
    ostringstream valueStream;
    valueStream << functions.blockJSON(block);
    db->Put(writeOptions, keyStream.str(), valueStream.str());

    // Close the database
    delete db; 
    return true;
}

/**
* AddBlock
*
* Description: Add a block json to the database. Indexed by block number.
*
* @param: CFunctions::block_structure block - structure representing block information.
* @return bool returns 1 is successfull.
*/
bool CBlockDB::AddBlock(CFunctions::block_structure block){
    CPlatform platform;
    CFunctions functions;
    std::string dbPath = platform.getSafirePath();
    //std::cout << dbPath << std::endl;

    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './blockdb'" << endl;
        cerr << status.ToString() << endl;
        return -1;
    }

    leveldb::WriteOptions writeOptions;

    // Insert block record into leveldb
    ostringstream keyStream;
    keyStream << "b_" << boost::lexical_cast<std::string>(block.number);
    ostringstream valueStream;
    valueStream << functions.blockJSON(block);
    db->Put(writeOptions, keyStream.str(), valueStream.str());

    // Insert all sender_key->block_id records for fast lookup.
    for(int i = 0; i < block.records.size(); i++ ){
        CFunctions::record_structure record = block.records.at(i);
        ostringstream sendKeyStream;
        sendKeyStream << "s_" << record.sender_public_key;
        ostringstream sendValueStream;
        sendValueStream << functions.blockJSON(block);
        db->Put(writeOptions, sendKeyStream.str(), sendValueStream.str());
    }
    
    // Close the database
    delete db;
    return true;
}


/**
* GetBlocks
*
* Description: get a list of blocks from the DB.
*
* @return: 
*/
void CBlockDB::GetBlocks()
{
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './blockdb'" << endl;
        cerr << status.ToString() << endl;
        return;
    }


    // Iterate over each item in the database and print them
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        cout << it->key().ToString() << " : " << it->value().ToString() << endl;
    }    
    if (false == it->status().ok())
    {
        cerr << "An error was found during the scan" << endl;
        cerr << it->status().ToString() << endl; 
    }

    // Close the database
    delete db; 
}


/**
* getFirstBlock
*
* // first_block
*/
CFunctions::block_structure CBlockDB::getFirstBlock(){
    CFunctions::block_structure block;
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './blockdb'" << endl;
        cerr << status.ToString() << endl;
        return block;
    }

    std::string key = "first_block"; // boost::lexical_cast<std::string>(number);
    std::string blockJson;
    db->Get(leveldb::ReadOptions(), key, &blockJson);

    CFunctions functions;
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(blockJson);

    if(blocks.size() > 0){
        block = blocks.at(0);
    }

    // Close the database
    delete db;

    return block;
}


/**
 * getBlock
 *
 * Description: Get a block by number
 *
 * @param: number (long) id of block to look up as a key
 */
CFunctions::block_structure CBlockDB::getBlock(long number){
    CFunctions::block_structure block;
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './blockdb'" << endl;
        cerr << status.ToString() << endl;
        return block;
    }

    std::string key = "b_" + boost::lexical_cast<std::string>(number);
    std::string blockJson;
    db->Get(leveldb::ReadOptions(), key, &blockJson);

    CFunctions functions;
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(blockJson);

    if(blocks.size() > 0){
        block = blocks.at(0);
    }

    // Close the database
    delete db;

    return block;
}

/**
 * GetBlockWithSender
 *
 * Description: Return a block record that contains a sender key in it.
 *  If no block exists, return null.
 *  This can be used to look up a block record validating a sender has been approved by the network.
 *
 * @param sender_key string.
 * @param index int.
 * @return block_structure.
 */
CFunctions::block_structure CBlockDB::GetBlockWithSender( std::string sender_key, int index ){
    CFunctions::block_structure block;
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './blockdb'" << endl;
        cerr << status.ToString() << endl;
        return block;
    }
    
    
    
    return block;
}
    
    

