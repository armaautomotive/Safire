// Copyright (c) 2016 2017 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockdb.h"
#include <sstream>
#include <unistd.h>   // open and close
#include "leveldb/db.h"
#include "platform.h"
#include "log.h";
#include <boost/lexical_cast.hpp>

using namespace std;

leveldb::DB * CBlockDB::db;

CBlockDB::CBlockDB(){
    //std::cout << "CBlockDB()\n";
    //CBlockDB::db = getDatabase();
}

CBlockDB::~CBlockDB(){
    //std::cout << "~CBlockDB()\n";
    //if(CBlockDB::db){
    //    std::cout << "   Delete db handler. \n";
        //delete CBlockDB::db;
    //}
}

/**
 * getDatabase
 *
 * Description: Get a levelDB instance.
 */
leveldb::DB * CBlockDB::getDatabase(){
    if(CBlockDB::db){
        return CBlockDB::db;
    }
    
    leveldb::DB* ldb;
    
    CPlatform platform;
    std::string dbPath = platform.getSafirePath();
    //std::cout << dbPath << std::endl;
    
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &ldb);
    if (false == status.ok())
    {
        cerr << "Unable to open/create database './blockdb'" << endl;
        cerr << status.ToString() << endl;
        //return 0;
        //CBlockDB::db = 0;
        ldb = 0;
    }
    
    CBlockDB::db = ldb;
    
    return ldb;
}

leveldb::DB * CBlockDB::getDatabase2(){
    leveldb::DB* ldb;
    CPlatform platform;
    std::string dbPath = platform.getSafirePath();
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &ldb);
    return ldb;
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
    CFunctions functions;
    CFileLogger log;
    
    /*
    CPlatform platform;
    std::string dbPath = platform.getSafirePath();
    //std::cout << dbPath << std::endl;
    */
    leveldb::WriteOptions writeOptions;
    leveldb::DB * db = getDatabase();
    if(!db){
        std::cout << "Error: CBlockDB::AddBlock \n";
        return false;
    }
   
    // Insert block record into leveldb
    ostringstream keyStream;
    keyStream << "b_" << boost::lexical_cast<std::string>(block.number);
    ostringstream valueStream;
    valueStream << functions.blockJSON(block);
    
    //std::cout << " key: " << keyStream.str() << " \n";
    //std::cout << " val: " << valueStream.str() << " \n";
    log.log("AddBlock to levelDB: \n");
    log.log("     key: " + keyStream.str() + "\n");
    log.log("     val: " + valueStream.str() + "\n");
    
    db->Put(writeOptions, keyStream.str(), valueStream.str());
    

    // Insert all sender_key->block_id records for fast lookup.
    for(int i = 0; i < block.records.size(); i++ ){
        CFunctions::record_structure record = block.records.at(i);
        //ostringstream sendKeyStream;
        //sendKeyStream << "s_" << record.sender_public_key;
        //ostringstream sendValueStream;
        //sendValueStream << functions.blockJSON(block);
        //db->Put(writeOptions, sendKeyStream.str(), sendValueStream.str());
    }
    
    // Insert all recipient_public_key->block_id records for fast lookup.
    for(int i = 0; i < block.records.size(); i++ ){
        
    }
    
    // Save index to next block
    if(block.previous_block_id > -1){
        ostringstream keyStream;
        keyStream << "next_block_" << block.previous_block_id;
        ostringstream valueStream;
        valueStream << block.number;
        db->Put(writeOptions, keyStream.str(), valueStream.str());
    }
    
    // Close the database
    //delete db;
    
    // Update latest block id record.
    // DON'T DO THIS! latest block should be latest validated block. not any random block.
    //long latestBlockId = getLatestBlockId();
    //if(block.number > latestBlockId){
        //setLatestBlockId(block.number);
    //}
    
    return true;
}

/**
 * setFirstBlockId
 *
 * Description: Set the dedicated entity recording the network wide first genesis block by id.
 *  Nodes that create a network will set this. Nodes that startup without this value need
 *  to retrieve past block data starting from this first block.
 */
void CBlockDB::setFirstBlockId(long number){
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    ostringstream keyStream;
    keyStream << "first_block_id";
    ostringstream valueStream;
    valueStream << boost::lexical_cast<std::string>(number);
    db->Put(writeOptions, keyStream.str(), valueStream.str());
    //delete db;
    
    //std::cout << " set first " << number << "\n";
}

/**
 * getFirstBlockId
 *
 * Description: Retruen the stord first block id. Represents the start of the chain.
 *   If this is missing the node has to:
 * A) retrieve the chain from other nodes or
 * B) start a new network.
 */
long CBlockDB::getFirstBlockId(){
    //std::cout << "  getFirstBlockId \n";
    leveldb::WriteOptions writeOptions;
    leveldb::DB * db = getDatabase();
    if(!db){
        std::cout << "Error: CBlockDB::getFirstBlockId \n";
        return -1;
    }
    std::string key = "first_block_id";
    std::string firstBlockIdString;
    db->Get(leveldb::ReadOptions(), key, &firstBlockIdString);
    long result = -1;
    if(firstBlockIdString.length() > 0){
        //result = std::stol(firstBlockIdString);
        result = boost::lexical_cast<long>(firstBlockIdString);
        //std::cout << "   --- " << firstBlockIdString << "\n";
    }
    //delete db;
    
    //std::cout << " get first " << result << "\n";
    
    return result;
}

/**
 * setLatestBlockId
 *
 * Description: set a keyed record with a reference to the latest verified block in
 *  the chain.
 * @param long number - block number.
 */
void CBlockDB::setLatestBlockId(long number){
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    ostringstream keyStream;
    keyStream << "latest_block_id";
    ostringstream valueStream;
    valueStream << boost::lexical_cast<std::string>(number);
    db->Put(writeOptions, keyStream.str(), valueStream.str());
    //delete db;
}

/**
 * getLatestBlockId
 *
 * Description: query the LevelDB for the keyed record storing the latest verified
 *  block number.
 * @return long block number.
 */
long CBlockDB::getLatestBlockId(){
    //std::cout << " a \n ";
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    std::string key = "latest_block_id"; // boost::lexical_cast<std::string>(number);
    std::string latestBlockIdString;
    db->Get(leveldb::ReadOptions(), key, &latestBlockIdString);
    
    //delete db;
    
    long result = -1;
    //std::stol(firstBlockId);
    if(latestBlockIdString.length() > 0){
        result = boost::lexical_cast<long>(latestBlockIdString);
    }
    
    //std::cout << " z \n ";
    return result;
}

/**
 * getNextBlockId
 *
 * Description: Get a block id for the block that follows after a given block id.
 * Blocks are generated in sequance but there may be ommitions if a node does not generate
 * a block. Each block contains the previous block id and a db index stores these in an index
 * for fast lookup.
 * @param previousBlockId - block number id.
 */
long CBlockDB::getNextBlockId(long previousBlockId){
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    ostringstream keyStream;
    keyStream << "next_block_" << previousBlockId;
    std::string nextBlockIdString;
    db->Get(leveldb::ReadOptions(), keyStream.str(), &nextBlockIdString);
    //delete db;
    long result = -1; //std::stol(nextBlockIdString);
    if(nextBlockIdString.length() > 0){
        result = boost::lexical_cast<long>(nextBlockIdString);
    }
    //std::cout << " next " << result << "\n";
    
    return result;
}

/**
* GetBlocks
*
* Description: get a list of blocks from the DB.
*
* @return: 
*/
void CBlockDB::GetBlocks(){
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();

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
    //delete it;

    // Close the database
    //delete db;
}

/**
 * getBlock
 *
 * Description: Get a block by number
 *
 * @param: number (long) id of block to look up as a key
 * @return block_structure record containing block member data.
 */
CFunctions::block_structure CBlockDB::getBlock(long number){
    CFunctions::block_structure block;
    block.number = -1;
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();

    std::string key = "b_" + boost::lexical_cast<std::string>(number);
    std::string blockJson;
    db->Get(leveldb::ReadOptions(), key, &blockJson);

    CFunctions functions;
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(blockJson);

    if(blocks.size() > 0){
        block = blocks.at(0);
    }

    // Close the database
    //delete db;
    return block;
}

/**
 * getNextBlock
 *
 * Description: get the next block in the chain.
 */
CFunctions::block_structure CBlockDB::getNextBlock(CFunctions::block_structure block){
    long nextId = getNextBlockId(block.number);
    CFunctions::block_structure nextBlock = getBlock(nextId);
    return nextBlock;
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
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    
    // todo:
    
    //delete db;
    return block;
}

/**
 * DeleteAll
 *
 * Description: Delete all data in the levelDB database.
 */
void CBlockDB::DeleteAll(){
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    // Iterate over each item in the database and print them
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        //cout << it->key().ToString() << " : " << it->value().ToString() << endl;
        db->Delete(leveldb::WriteOptions(), it->key().ToString());
        cout << "Delete: " << it->key().ToString() << "\n";
    }
    if (false == it->status().ok())
    {
        cerr << "An error was found during the scan" << endl;
        cerr << it->status().ToString() << endl;
    }
    delete it;
    //delete db;
}
