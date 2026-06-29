// Copyright (c) 2016 2017 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Rename chainDB ???
// Add indexDB ???

#include "blockdb.h"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <unistd.h>   // open and close
#include "leveldb/db.h"
#include "platform.h"
#include "log.h";
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

using namespace std;

leveldb::DB * CBlockDB::db;

namespace {

bool preferBlockVariant(const CFunctions::block_structure& candidate, const CFunctions::block_structure& current){
    if(current.number <= 0){
        return true;
    }
    if(candidate.records.size() != current.records.size()){
        return candidate.records.size() > current.records.size();
    }
    return candidate.hash.compare(current.hash) < 0;
}

std::string forkBlockKey(const CFunctions::block_structure& block){
    std::stringstream ss;
    ss << "slot:" << block.number << ":" << block.hash;
    return ss.str();
}

std::string blockHashKey(const std::string& hash){
    return "block:" + hash;
}

std::string canonicalHashKey(long number){
    std::stringstream ss;
    ss << "canonical:" << number;
    return ss.str();
}

}

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
        if(status.ToString().find("LOCK") != std::string::npos){
            cerr << "Another Safire process is probably already using this blockdb. Stop it before starting another node in this directory." << endl;
        }
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

    if(block.previous_block_id >= block.number && block.previous_block_id != -1){
        log.log("Reject block: previous block id is not before block number.\n");
        return false;
    }

    if(block.records.size() > CFunctions::MAX_BLOCK_RECORDS){
        log.log("Reject block: too many records.\n");
        return false;
    }
    for(int i = 0; i < block.records.size(); i++){
        if(functions.isRecordSizeValid(block.records.at(i)) == false){
            log.log("Reject block: oversized record.\n");
            return false;
        }
    }

    ostringstream valueStream;
    valueStream << functions.blockJSON(block);
    if(valueStream.str().length() > CFunctions::MAX_BLOCK_JSON_BYTES){
        log.log("Reject block: serialized block is too large.\n");
        return false;
    }

    std::string hashKey = blockHashKey(block.hash);
    std::string existingHashJson;
    db->Get(leveldb::ReadOptions(), hashKey, &existingHashJson);
    if(existingHashJson.length() > 0){
        log.log("FYI block hash allready exists... \n");
        return true;
    }

    db->Put(writeOptions, hashKey, valueStream.str());
    db->Put(writeOptions, forkBlockKey(block), block.hash);

    CFunctions::block_structure canonicalBlock = getBlock(block.number);
    bool shouldWriteCanonical = canonicalBlock.number <= 0 || preferBlockVariant(block, canonicalBlock);
    if(shouldWriteCanonical){
        db->Put(writeOptions, canonicalHashKey(block.number), block.hash);
        ostringstream legacyCanonicalKey;
        legacyCanonicalKey << "b_" << boost::lexical_cast<std::string>(block.number);
        db->Put(writeOptions, legacyCanonicalKey.str(), valueStream.str());
    }

    if(canonicalBlock.number > 0){
        log.log("Store same-slot block variant: " + hashKey + "\n");
        return true;
    }
   
    // Save index to next block
    if(block.previous_block_id > -1){
        ostringstream keyStream;
        keyStream << "next_block_" << block.previous_block_id;
        ostringstream valueStream;
        valueStream << block.number;
        std::string existingNextBlockId;
        db->Get(leveldb::ReadOptions(), keyStream.str(), &existingNextBlockId);
        bool shouldWriteNextIndex = existingNextBlockId.length() == 0 || existingNextBlockId.compare(valueStream.str()) == 0;
        if(shouldWriteNextIndex == false){
            long existingNextId = std::atol(existingNextBlockId.c_str());
            CFunctions::block_structure existingNextBlock = getBlock(existingNextId);
            if(existingNextBlock.number <= 0){
                shouldWriteNextIndex = true;
                log.log("Repair stale next-block index.\n");
            }
        }
        if(shouldWriteNextIndex){
            db->Put(writeOptions, keyStream.str(), valueStream.str());
        }
    }

    //std::cout << " key: " << keyStream.str() << " \n";
    //std::cout << " val: " << valueStream.str() << " \n";
    log.log("AddBlock to levelDB: \n");
    log.log("     key: " + hashKey + "\n");
    log.log("     val: " + valueStream.str() + "\n");
    

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
    if(!db){
        std::cout << "Error: CBlockDB::setFirstBlockId \n";
        return;
    }
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
    if(!db){
        std::cout << "Error: CBlockDB::setLatestBlockId \n";
        return;
    }
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
    if(!db){
        std::cout << "Error: CBlockDB::getLatestBlockId \n";
        return -1;
    }
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
 * getConnectedLatestBlockId
 *
 * Description: Return the latest block reachable from the genesis block by
 * following next_block indexes. Stored blocks that are not connected to the
 * accepted chain do not count as the chain tip.
 */
long CBlockDB::getConnectedLatestBlockId(){
    long firstBlockId = getFirstBlockId();
    if(firstBlockId < 0){
        return -1;
    }

    CFunctions::block_structure block = getBlock(firstBlockId);
    long latestConnectedBlockId = block.number;
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        latestConnectedBlockId = block.number;
        CFunctions::block_structure nextBlock = getNextBlock(block);
        if(nextBlock.number <= 0 || nextBlock.number == block.number){
            break;
        }
        block = nextBlock;
        guard++;
    }

    return latestConnectedBlockId;
}

long CBlockDB::rebuildBestChainIndex(){
    long firstBlockId = getFirstBlockId();
    if(firstBlockId < 0){
        return -1;
    }
    long currentConnectedLatestBlockId = getConnectedLatestBlockId();
    long currentConnectedBlockCount = 0;
    CFunctions::block_structure currentIndexedBlock = getBlock(firstBlockId);
    int currentGuard = 0;
    while(currentIndexedBlock.number > 0 && currentGuard < 100000){
        currentConnectedBlockCount++;
        CFunctions::block_structure nextIndexedBlock = getNextBlock(currentIndexedBlock);
        if(nextIndexedBlock.number <= 0 || nextIndexedBlock.number == currentIndexedBlock.number){
            break;
        }
        currentIndexedBlock = nextIndexedBlock;
        currentGuard++;
    }

    leveldb::DB* db = getDatabase();
    if(!db){
        return -1;
    }

    std::vector<CFunctions::block_structure> blocks = getStoredBlocks();
    if(blocks.size() == 0){
        return -1;
    }

    std::map<long, CFunctions::block_structure> blockByNumber;
    std::map<long, std::vector<CFunctions::block_structure> > childrenByParent;
    for(int i = 0; i < blocks.size(); i++){
        CFunctions::block_structure block = blocks.at(i);
        if(preferBlockVariant(block, blockByNumber[block.number])){
            blockByNumber[block.number] = block;
        }
    }

    blocks.clear();
    for(std::map<long, CFunctions::block_structure>::iterator it = blockByNumber.begin(); it != blockByNumber.end(); ++it){
        CFunctions::block_structure block = it->second;
        blocks.push_back(block);
        if(block.previous_block_id > 0 && block.previous_block_id < block.number){
            childrenByParent[block.previous_block_id].push_back(block);
        }
    }

    std::sort(blocks.begin(), blocks.end(),
        [](const CFunctions::block_structure& a, const CFunctions::block_structure& b){
            return a.number > b.number;
        });

    std::map<long, long> bestTipByBlock;
    std::map<long, long> bestLengthByBlock;
    std::map<long, long> bestChildByBlock;
    for(int i = 0; i < blocks.size(); i++){
        CFunctions::block_structure block = blocks.at(i);
        long bestTip = block.number;
        long bestLength = 1;
        long bestChild = -1;
        std::vector<CFunctions::block_structure> children = childrenByParent[block.number];
        for(int c = 0; c < children.size(); c++){
            CFunctions::block_structure child = children.at(c);
            long childTip = child.number;
            if(bestTipByBlock.find(child.number) != bestTipByBlock.end()){
                childTip = bestTipByBlock[child.number];
            }
            long childLength = 1;
            if(bestLengthByBlock.find(child.number) != bestLengthByBlock.end()){
                childLength = bestLengthByBlock[child.number];
            }
            long candidateLength = childLength + 1;
            if(candidateLength > bestLength ||
               (candidateLength == bestLength && childTip > bestTip) ||
               (candidateLength == bestLength && childTip == bestTip && (bestChild < 0 || child.number < bestChild))){
                bestTip = childTip;
                bestLength = candidateLength;
                bestChild = child.number;
            }
        }
        bestTipByBlock[block.number] = bestTip;
        bestLengthByBlock[block.number] = bestLength;
        if(bestChild > 0){
            bestChildByBlock[block.number] = bestChild;
        }
    }

    long latestBestBlockId = firstBlockId;
    long bestChainBlockCount = 0;
    long currentBlockId = firstBlockId;
    int guard = 0;
    while(currentBlockId > 0 && guard < 100000){
        latestBestBlockId = currentBlockId;
        bestChainBlockCount++;
        if(bestChildByBlock.find(currentBlockId) == bestChildByBlock.end()){
            break;
        }
        currentBlockId = bestChildByBlock[currentBlockId];
        guard++;
    }

    if(currentConnectedBlockCount > bestChainBlockCount ||
       (currentConnectedBlockCount == bestChainBlockCount && currentConnectedLatestBlockId >= latestBestBlockId)){
        setLatestBlockId(currentConnectedLatestBlockId);
        return currentConnectedLatestBlockId;
    }

    leveldb::WriteOptions writeOptions;
    std::vector<std::string> nextIndexKeys;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for(it->SeekToFirst(); it->Valid(); it->Next()){
        std::string key = it->key().ToString();
        if(boost::algorithm::starts_with(key, "next_block_")){
            nextIndexKeys.push_back(key);
        }
    }
    delete it;

    for(int i = 0; i < nextIndexKeys.size(); i++){
        db->Delete(writeOptions, nextIndexKeys.at(i));
    }

    currentBlockId = firstBlockId;
    guard = 0;
    while(currentBlockId > 0 && guard < 100000){
        if(blockByNumber.find(currentBlockId) != blockByNumber.end()){
            ostringstream blockKeyStream;
            blockKeyStream << "b_" << currentBlockId;
            CFunctions functions;
            CFunctions::block_structure canonicalBlock = blockByNumber[currentBlockId];
            db->Put(writeOptions, canonicalHashKey(currentBlockId), canonicalBlock.hash);
            db->Put(writeOptions, blockHashKey(canonicalBlock.hash), functions.blockJSON(canonicalBlock));
            db->Put(writeOptions, blockKeyStream.str(), functions.blockJSON(canonicalBlock));
        }
        if(bestChildByBlock.find(currentBlockId) == bestChildByBlock.end()){
            break;
        }
        long childBlockId = bestChildByBlock[currentBlockId];
        ostringstream keyStream;
        keyStream << "next_block_" << currentBlockId;
        ostringstream valueStream;
        valueStream << childBlockId;
        db->Put(writeOptions, keyStream.str(), valueStream.str());
        currentBlockId = childBlockId;
        guard++;
    }

    setLatestBlockId(latestBestBlockId);
    return latestBestBlockId;
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
    if(!db){
        std::cout << "Error: CBlockDB::getNextBlockId \n";
        return -1;
    }
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
    if(!db){
        std::cout << "Error: CBlockDB::GetBlocks \n";
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
    //delete it;

    // Close the database
    //delete db;
}

std::vector<CFunctions::block_structure> CBlockDB::getStoredBlocks(){
    std::vector<CFunctions::block_structure> blocks;
    leveldb::DB* db = getDatabase();
    if(!db){
        return blocks;
    }

    CFunctions functions;
    std::set<std::string> seenHashes;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::string key = it->key().ToString();
        if(boost::algorithm::starts_with(key, "block:") ||
           boost::algorithm::starts_with(key, "b_") ||
           boost::algorithm::starts_with(key, "bf_")){
            std::vector<CFunctions::block_structure> parsedBlocks = functions.parseBlockJson(it->value().ToString());
            for(int i = 0; i < parsedBlocks.size(); i++){
                CFunctions::block_structure block = parsedBlocks.at(i);
                if(block.number > 0 && seenHashes.find(block.hash) == seenHashes.end()){
                    seenHashes.insert(block.hash);
                    blocks.push_back(block);
                }
            }
        }
    }
    delete it;
    return blocks;
}

/**
 * getBlockByHash
 *
 * Description: Get a block by its content hash.
 */
CFunctions::block_structure CBlockDB::getBlockByHash(std::string hash){
    CFunctions::block_structure block;
    block.number = -1;
    if(hash.length() == 0){
        return block;
    }

    leveldb::DB* db = getDatabase();
    if(!db){
        std::cout << "Error: CBlockDB::getBlockByHash \n";
        return block;
    }
    std::string blockJson;
    db->Get(leveldb::ReadOptions(), blockHashKey(hash), &blockJson);

    CFunctions functions;
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(blockJson);
    if(blocks.size() > 0){
        block = blocks.at(0);
    }
    return block;
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
    if(!db){
        std::cout << "Error: CBlockDB::getBlock \n";
        return block;
    }

    std::string canonicalHash;
    db->Get(leveldb::ReadOptions(), canonicalHashKey(number), &canonicalHash);
    if(canonicalHash.length() > 0){
        block = getBlockByHash(canonicalHash);
        if(block.number > 0){
            return block;
        }
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
    block.number = -1;
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    if(!db){
        std::cout << "Error: CBlockDB::GetBlockWithSender \n";
        return block;
    }
    
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
    if(!db){
        std::cout << "Error: CBlockDB::DeleteAll \n";
        return;
    }
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
