// Copyright (c) 2016 2017 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Rename chainDB ???
// Add indexDB ???

#include "blockdb.h"
#include <algorithm>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <unistd.h>   // open and close
#include "leveldb/db.h"
#include "platform.h"
#include "log.h";
#include "ecdsacrypto.h"
#include "networkconfig.h"
#include "functions/chainvalidator.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

using namespace std;

leveldb::DB * CBlockDB::db;
bool CBlockDB::schemaMigrated = false;
const int CBlockDB::CURRENT_SCHEMA_VERSION;

namespace {

std::mutex g_bestChainRebuildMutex;
const std::size_t MAX_REBUILD_CHAIN_CANDIDATES = 256;
const std::size_t MAX_REBUILD_CHAIN_DEPTH = 20000;
const long VALIDATED_CHECKPOINT_DEPTH = 2000;
const long CHECKPOINT_FAST_VALIDATE_LIMIT = 20000;

std::string blockHashKey(const std::string& hash);
std::string canonicalHashKey(long number);

std::string chainMetaKey(const std::string& name){
    return "chain:" + name;
}

std::string nextHashKey(const std::string& parentHash){
    return "next:" + parentHash;
}

bool parseLongValue(const std::string& text, long& value){
    if(text.length() == 0){
        return false;
    }
    char* end = 0;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if(end == text.c_str() || *end != '\0'){
        return false;
    }
    value = parsed;
    return true;
}

bool readKey(leveldb::DB* db, const std::string& key, std::string& value){
    value = "";
    if(!db){
        return false;
    }
    leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &value);
    return status.ok() && value.length() > 0;
}

bool readLongKey(leveldb::DB* db, const std::string& key, long& value){
    std::string text;
    if(readKey(db, key, text) == false){
        return false;
    }
    return parseLongValue(text, value);
}

CFunctions::block_structure parseSingleBlockJson(const std::string& json){
    CFunctions::block_structure block;
    block.number = -1;
    if(json.length() == 0){
        return block;
    }
    CFunctions functions;
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(json);
    if(blocks.size() > 0){
        block = blocks.at(0);
    }
    return block;
}

CFunctions::block_structure rawBlockByHash(leveldb::DB* db, const std::string& hash){
    std::string blockJson;
    if(readKey(db, blockHashKey(hash), blockJson) == false){
        CFunctions::block_structure block;
        block.number = -1;
        return block;
    }
    return parseSingleBlockJson(blockJson);
}

std::string rawCanonicalHash(leveldb::DB* db, long number){
    std::string hash;
    if(readKey(db, canonicalHashKey(number), hash)){
        return hash;
    }

    std::string blockJson;
    std::stringstream legacyKey;
    legacyKey << "b_" << number;
    if(readKey(db, legacyKey.str(), blockJson)){
        CFunctions::block_structure block = parseSingleBlockJson(blockJson);
        if(block.hash.length() > 0){
            return block.hash;
        }
    }
    return "";
}

bool preferBlockVariant(const CFunctions::block_structure& candidate, const CFunctions::block_structure& current){
    if(current.number <= 0){
        return true;
    }
    if(candidate.records.size() != current.records.size()){
        return candidate.records.size() > current.records.size();
    }
    return candidate.hash.compare(current.hash) < 0;
}

bool childConnectsToParent(const CFunctions::block_structure& child, const CFunctions::block_structure& parent){
    if(child.number <= 0 || parent.number <= 0){
        return false;
    }
    if(child.previous_block_id != parent.number){
        return false;
    }
    if(child.previous_block_id >= child.number){
        return false;
    }
    if(child.previous_block_hash.length() == 0 || parent.hash.length() == 0){
        return false;
    }
    return child.previous_block_hash.compare(parent.hash) == 0;
}

bool preferChainChild(
    const CFunctions::block_structure& candidate,
    const CFunctions::block_structure& current,
    const std::map<std::string, long>& bestLengthByHash,
    const std::map<std::string, long>& bestTipByHash
){
    if(current.number <= 0){
        return true;
    }

    long candidateLength = 1;
    std::map<std::string, long>::const_iterator candidateLengthIt = bestLengthByHash.find(candidate.hash);
    if(candidateLengthIt != bestLengthByHash.end()){
        candidateLength = candidateLengthIt->second;
    }

    long currentLength = 1;
    std::map<std::string, long>::const_iterator currentLengthIt = bestLengthByHash.find(current.hash);
    if(currentLengthIt != bestLengthByHash.end()){
        currentLength = currentLengthIt->second;
    }

    if(candidateLength != currentLength){
        return candidateLength > currentLength;
    }

    long candidateTip = candidate.number;
    std::map<std::string, long>::const_iterator candidateTipIt = bestTipByHash.find(candidate.hash);
    if(candidateTipIt != bestTipByHash.end()){
        candidateTip = candidateTipIt->second;
    }

    long currentTip = current.number;
    std::map<std::string, long>::const_iterator currentTipIt = bestTipByHash.find(current.hash);
    if(currentTipIt != bestTipByHash.end()){
        currentTip = currentTipIt->second;
    }

    if(candidateTip != currentTip){
        return candidateTip > currentTip;
    }

    return preferBlockVariant(candidate, current);
}

bool preferChainPath(
    const std::vector<CFunctions::block_structure>& candidate,
    const std::vector<CFunctions::block_structure>& current
){
    if(current.size() == 0){
        return true;
    }
    if(candidate.size() != current.size()){
        return candidate.size() > current.size();
    }

    const CFunctions::block_structure& candidateTip = candidate.back();
    const CFunctions::block_structure& currentTip = current.back();
    if(candidateTip.number != currentTip.number){
        return candidateTip.number > currentTip.number;
    }
    return candidateTip.hash.compare(currentTip.hash) < 0;
}

void collectReachableChainCandidates(
    const CFunctions::block_structure& block,
    const std::map<std::string, std::vector<CFunctions::block_structure> >& childrenByParentHash,
    std::vector<CFunctions::block_structure>& path,
    std::set<std::string>& pathHashes,
    std::vector<std::vector<CFunctions::block_structure> >& candidates
){
    if(candidates.size() >= MAX_REBUILD_CHAIN_CANDIDATES){
        return;
    }
    if(block.hash.length() == 0 || pathHashes.find(block.hash) != pathHashes.end()){
        return;
    }

    path.push_back(block);
    pathHashes.insert(block.hash);

    if(path.size() >= MAX_REBUILD_CHAIN_DEPTH){
        candidates.push_back(path);
        pathHashes.erase(block.hash);
        path.pop_back();
        return;
    }

    std::map<std::string, std::vector<CFunctions::block_structure> >::const_iterator childrenIt = childrenByParentHash.find(block.hash);
    if(childrenIt != childrenByParentHash.end() && childrenIt->second.size() > 0){
        std::vector<CFunctions::block_structure> children = childrenIt->second;
        std::sort(children.begin(), children.end(),
            [](const CFunctions::block_structure& a, const CFunctions::block_structure& b){
                if(a.number != b.number){
                    return a.number < b.number;
                }
                return a.hash.compare(b.hash) < 0;
            });
        for(int i = 0; i < children.size(); ++i){
            if(candidates.size() >= MAX_REBUILD_CHAIN_CANDIDATES){
                break;
            }
            collectReachableChainCandidates(children.at(i), childrenByParentHash, path, pathHashes, candidates);
        }
    } else {
        candidates.push_back(path);
    }

    pathHashes.erase(block.hash);
    path.pop_back();
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

std::string reorgMetaKey(const std::string& name){
    return "last_reorg_" + name;
}

std::string forkVariantCountKey(){
    return "fork_variant_count";
}

std::string forkVariantLatestBlockKey(){
    return "fork_variant_latest_block";
}

struct ForkVariantStats {
    long count;
    long latest_block;
};

ForkVariantStats scanForkVariantStats(leveldb::DB* db){
    ForkVariantStats stats;
    stats.count = 0;
    stats.latest_block = -1;
    if(!db){
        return stats;
    }

    std::map<long, long> variantsByBlock;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for(it->SeekToFirst(); it->Valid(); it->Next()){
        std::string key = it->key().ToString();
        if(boost::algorithm::starts_with(key, "slot:")){
            std::size_t firstColon = key.find(":");
            std::size_t secondColon = key.find(":", firstColon + 1);
            if(firstColon != std::string::npos && secondColon != std::string::npos){
                std::string blockNumber = key.substr(firstColon + 1, secondColon - firstColon - 1);
                variantsByBlock[boost::lexical_cast<long>(blockNumber)]++;
            }
        }
    }
    delete it;

    for(std::map<long, long>::iterator it = variantsByBlock.begin(); it != variantsByBlock.end(); ++it){
        if(it->second > 1){
            stats.count += it->second - 1;
            if(it->first > stats.latest_block){
                stats.latest_block = it->first;
            }
        }
    }
    return stats;
}

long scanForkVariantCount(leveldb::DB* db){
    ForkVariantStats stats = scanForkVariantStats(db);
    return stats.count;
}

bool hasForkVariantAtOrAfter(leveldb::DB* db, long minimumBlock){
    if(!db || minimumBlock <= 0){
        return false;
    }

    std::map<long, long> variantsByBlock;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for(it->SeekToFirst(); it->Valid(); it->Next()){
        std::string key = it->key().ToString();
        if(boost::algorithm::starts_with(key, "slot:") == false){
            continue;
        }

        std::size_t firstColon = key.find(":");
        std::size_t secondColon = key.find(":", firstColon + 1);
        if(firstColon == std::string::npos || secondColon == std::string::npos){
            continue;
        }

        long blockNumber = -1;
        std::string blockNumberText = key.substr(firstColon + 1, secondColon - firstColon - 1);
        if(parseLongValue(blockNumberText, blockNumber) == false || blockNumber < minimumBlock){
            continue;
        }

        variantsByBlock[blockNumber]++;
        if(variantsByBlock[blockNumber] > 1){
            delete it;
            return true;
        }
    }
    delete it;
    return false;
}

bool blockEnvelopeIsValid(const CFunctions::block_structure& block){
    if(block.number <= 0 || block.hash.length() == 0 || block.creator_key.length() == 0){
        return false;
    }
    if(block.records.size() > CFunctions::MAX_BLOCK_RECORDS){
        return false;
    }

    CFunctions functions;
    if(block.records_merkle_root.length() > 0 &&
       functions.getRecordsMerkleRoot(block.records).compare(block.records_merkle_root) != 0){
        return false;
    }
    if(functions.getBlockHash(block).compare(block.hash) != 0){
        return false;
    }

    CECDSACrypto ecdsa;
    return block.signature.length() > 0 &&
        ecdsa.VerifyMessageCompressed(block.hash, block.signature, block.creator_key) == 1;
}

bool readValidatedCheckpoint(leveldb::DB* db, long& checkpointBlock, std::string& checkpointHash){
    checkpointBlock = -1;
    checkpointHash = "";
    if(readLongKey(db, chainMetaKey("validated_checkpoint_block"), checkpointBlock) == false){
        return false;
    }
    if(readKey(db, chainMetaKey("validated_checkpoint_hash"), checkpointHash) == false){
        return false;
    }
    return checkpointBlock > 0 && checkpointHash.length() > 0;
}

bool checkpointMatchesCanonical(leveldb::DB* db, long checkpointBlock, const std::string& checkpointHash, CFunctions::block_structure& checkpoint){
    checkpoint.number = -1;
    if(checkpointBlock <= 0 || checkpointHash.length() == 0){
        return false;
    }
    std::string canonicalHash = rawCanonicalHash(db, checkpointBlock);
    if(canonicalHash.compare(checkpointHash) != 0){
        return false;
    }
    checkpoint = rawBlockByHash(db, checkpointHash);
    return checkpoint.number == checkpointBlock &&
        checkpoint.hash.compare(checkpointHash) == 0 &&
        blockEnvelopeIsValid(checkpoint);
}

void maybeAdvanceValidatedCheckpoint(leveldb::DB* db, const CFunctions::block_structure& tip){
    if(!db || tip.number <= 0 || tip.hash.length() == 0 || blockEnvelopeIsValid(tip) == false){
        return;
    }

    long currentCheckpointBlock = -1;
    std::string currentCheckpointHash;
    bool hasCheckpoint = readValidatedCheckpoint(db, currentCheckpointBlock, currentCheckpointHash);
    if(hasCheckpoint && tip.number - currentCheckpointBlock < VALIDATED_CHECKPOINT_DEPTH * 2){
        return;
    }

    CFunctions::block_structure checkpoint = tip;
    long walked = 0;
    while(walked < VALIDATED_CHECKPOINT_DEPTH && checkpoint.previous_block_hash.length() > 0){
        CFunctions::block_structure previous = rawBlockByHash(db, checkpoint.previous_block_hash);
        if(childConnectsToParent(checkpoint, previous) == false || blockEnvelopeIsValid(previous) == false){
            break;
        }
        checkpoint = previous;
        walked++;
    }

    if(checkpoint.number <= 0 || checkpoint.hash.length() == 0){
        return;
    }
    if(hasCheckpoint && checkpoint.number <= currentCheckpointBlock){
        return;
    }

    leveldb::WriteOptions writeOptions;
    db->Put(writeOptions, chainMetaKey("validated_checkpoint_block"), boost::lexical_cast<std::string>(checkpoint.number));
    db->Put(writeOptions, chainMetaKey("validated_checkpoint_hash"), checkpoint.hash);
}

bool tryValidatedCheckpointFastPath(CBlockDB& blockDB, leveldb::DB* db, long& latestBestBlockId){
    latestBestBlockId = -1;
    if(!db){
        return false;
    }

    long latestBlockId = blockDB.getLatestBlockId();
    if(latestBlockId <= 0){
        return false;
    }

    long checkpointBlock = -1;
    std::string checkpointHash;
    if(readValidatedCheckpoint(db, checkpointBlock, checkpointHash) == false || latestBlockId < checkpointBlock){
        return false;
    }

    CFunctions::block_structure current;
    if(checkpointMatchesCanonical(db, checkpointBlock, checkpointHash, current) == false){
        return false;
    }

    std::string forkCountValue;
    long forkCount = 0;
    if(readKey(db, forkVariantCountKey(), forkCountValue) &&
       parseLongValue(forkCountValue, forkCount) &&
       forkCount > 0){
        long latestForkVariantBlock = -1;
        bool latestForkKnown = readLongKey(db, forkVariantLatestBlockKey(), latestForkVariantBlock);
        if((latestForkKnown == false || latestForkVariantBlock >= checkpointBlock) &&
           hasForkVariantAtOrAfter(db, checkpointBlock)){
            return false;
        }
    }

    long steps = 0;
    while(current.number < latestBlockId && steps < CHECKPOINT_FAST_VALIDATE_LIMIT){
        std::string nextHash;
        if(readKey(db, nextHashKey(current.hash), nextHash) == false){
            return false;
        }
        CFunctions::block_structure next = rawBlockByHash(db, nextHash);
        if(childConnectsToParent(next, current) == false || blockEnvelopeIsValid(next) == false){
            return false;
        }
        current = next;
        steps++;
    }

    if(current.number != latestBlockId || steps >= CHECKPOINT_FAST_VALIDATE_LIMIT){
        return false;
    }

    CFunctions::block_structure canonicalTip = blockDB.getBlock(latestBlockId);
    if(canonicalTip.number != latestBlockId || canonicalTip.hash.compare(current.hash) != 0){
        return false;
    }

    db->Put(leveldb::WriteOptions(), chainMetaKey("connected_latest"), boost::lexical_cast<std::string>(current.number));
    maybeAdvanceValidatedCheckpoint(db, current);
    latestBestBlockId = current.number;
    return true;
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

void CBlockDB::migrateSchema(leveldb::DB* ldb){
    if(!ldb || CBlockDB::schemaMigrated){
        return;
    }

    std::string schemaVersion;
    readKey(ldb, chainMetaKey("schema_version"), schemaVersion);
    if(schemaVersion.compare(boost::lexical_cast<std::string>(CBlockDB::CURRENT_SCHEMA_VERSION)) == 0){
        CBlockDB::schemaMigrated = true;
        return;
    }

    leveldb::WriteOptions writeOptions;

    long firstBlockId = -1;
    if(readLongKey(ldb, chainMetaKey("first_block"), firstBlockId) == false &&
       readLongKey(ldb, "first_block_id", firstBlockId)){
        ldb->Put(writeOptions, chainMetaKey("first_block"), boost::lexical_cast<std::string>(firstBlockId));
    }

    long latestBlockId = -1;
    if(readLongKey(ldb, chainMetaKey("latest_block"), latestBlockId) == false &&
       readLongKey(ldb, "latest_block_id", latestBlockId)){
        ldb->Put(writeOptions, chainMetaKey("latest_block"), boost::lexical_cast<std::string>(latestBlockId));
    }

    if(latestBlockId > 0){
        std::string latestHash;
        if(readKey(ldb, chainMetaKey("latest_hash"), latestHash) == false){
            latestHash = rawCanonicalHash(ldb, latestBlockId);
            if(latestHash.length() > 0){
                ldb->Put(writeOptions, chainMetaKey("latest_hash"), latestHash);
            }
        }
    }

    std::vector<std::string> legacyNextKeys;
    leveldb::Iterator* it = ldb->NewIterator(leveldb::ReadOptions());
    for(it->SeekToFirst(); it->Valid(); it->Next()){
        std::string key = it->key().ToString();
        if(boost::algorithm::starts_with(key, "next_block_")){
            legacyNextKeys.push_back(key);
        }
    }
    delete it;

    for(int i = 0; i < legacyNextKeys.size(); ++i){
        std::string key = legacyNextKeys.at(i);
        std::string parentText = key.substr(std::string("next_block_").length());
        long parentId = -1;
        long childId = -1;
        std::string childText;
        if(parseLongValue(parentText, parentId) == false ||
           readKey(ldb, key, childText) == false ||
           parseLongValue(childText, childId) == false){
            continue;
        }

        CFunctions::block_structure parent = rawBlockByHash(ldb, rawCanonicalHash(ldb, parentId));
        CFunctions::block_structure child = rawBlockByHash(ldb, rawCanonicalHash(ldb, childId));
        if(parent.hash.length() > 0 && childConnectsToParent(child, parent)){
            ldb->Put(writeOptions, nextHashKey(parent.hash), child.hash);
        }
    }

    ldb->Put(writeOptions, chainMetaKey("schema_version"), boost::lexical_cast<std::string>(CBlockDB::CURRENT_SCHEMA_VERSION));
    CBlockDB::schemaMigrated = true;
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
    migrateSchema(ldb);
    
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

    if(block.previous_block_id <= 0){
        CNetworkConfig config = CNetworkConfig::load();
        if(config.genesisMatches(block.number, block.hash) == false){
            log.log("Reject block: genesis block does not match safire.conf.\n");
            return false;
        }
    }

    if(block.records.size() > CFunctions::MAX_BLOCK_RECORDS){
        log.log("Reject block: too many records.\n");
        return false;
    }
    std::set<std::string> recordHashes;
    for(int i = 0; i < block.records.size(); i++){
        CFunctions::record_structure record = block.records.at(i);
        if(functions.isRecordSizeValid(record) == false){
            log.log("Reject block: oversized record.\n");
            return false;
        }
        if(record.hash.length() > 0 && recordHashes.find(record.hash) != recordHashes.end()){
            log.log("Reject block: duplicate record hash.\n");
            return false;
        }
        if(record.hash.length() > 0){
            recordHashes.insert(record.hash);
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

    std::string validationReason;
    if(CChainValidator::validateBlockForStorage(*this, block, validationReason) == false){
        log.log("Reject block: " + validationReason + ".\n");
        return false;
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

    bool sameSlotVariant = canonicalBlock.number > 0;
    if(sameSlotVariant){
        log.log("Store same-slot block variant: " + hashKey + "\n");
        std::string forkCountValue;
        long forkCount = 0;
        db->Get(leveldb::ReadOptions(), forkVariantCountKey(), &forkCountValue);
        if(forkCountValue.length() > 0){
            forkCount = boost::lexical_cast<long>(forkCountValue);
            forkCount++;
        } else {
            forkCount = scanForkVariantCount(db);
        }
        db->Put(writeOptions, forkVariantCountKey(), boost::lexical_cast<std::string>(forkCount));
        long latestForkVariantBlock = -1;
        readLongKey(db, forkVariantLatestBlockKey(), latestForkVariantBlock);
        if(block.number > latestForkVariantBlock){
            db->Put(writeOptions, forkVariantLatestBlockKey(), boost::lexical_cast<std::string>(block.number));
        }
    }
    if(sameSlotVariant && shouldWriteCanonical == false){
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

        if(block.previous_block_hash.length() > 0){
            std::string existingNextHash;
            db->Get(leveldb::ReadOptions(), nextHashKey(block.previous_block_hash), &existingNextHash);
            bool shouldWriteHashNextIndex = existingNextHash.length() == 0 || existingNextHash.compare(block.hash) == 0;
            if(shouldWriteHashNextIndex == false){
                CFunctions::block_structure existingNextBlock = getBlockByHash(existingNextHash);
                if(childConnectsToParent(existingNextBlock, getBlockByHash(block.previous_block_hash)) == false){
                    shouldWriteHashNextIndex = true;
                    log.log("Repair stale next-hash index.\n");
                }
            }
            if(shouldWriteHashNextIndex){
                db->Put(writeOptions, nextHashKey(block.previous_block_hash), block.hash);
            }
        }
    }

    long latestBlockId = getLatestBlockId();
    if(block.previous_block_id <= 0 && latestBlockId < 0){
        setLatestBlockId(block.number);
    } else if(block.previous_block_id == latestBlockId){
        CFunctions::block_structure latestBlock = getBlock(latestBlockId);
        if(latestBlock.number == latestBlockId &&
           latestBlock.hash.length() > 0 &&
           latestBlock.hash.compare(block.previous_block_hash) == 0){
            setLatestBlockId(block.number);
            maybeAdvanceValidatedCheckpoint(db, block);
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
    
    // The chain tip is advanced above only when this block directly extends the
    // current canonical tip. Fork/repair cases still use rebuildBestChainIndex().
    
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
    db->Put(writeOptions, chainMetaKey("first_block"), valueStream.str());
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
    std::string firstBlockIdString;
    db->Get(leveldb::ReadOptions(), chainMetaKey("first_block"), &firstBlockIdString);
    if(firstBlockIdString.length() == 0){
        db->Get(leveldb::ReadOptions(), "first_block_id", &firstBlockIdString);
    }
    long result = -1;
    if(firstBlockIdString.length() > 0 && parseLongValue(firstBlockIdString, result) == false){
        //result = std::stol(firstBlockIdString);
        result = -1;
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
    db->Put(writeOptions, chainMetaKey("latest_block"), valueStream.str());
    CFunctions::block_structure latestBlock = getBlock(number);
    if(latestBlock.hash.length() > 0){
        db->Put(writeOptions, chainMetaKey("latest_hash"), latestBlock.hash);
    }
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
    std::string latestBlockIdString;
    db->Get(leveldb::ReadOptions(), chainMetaKey("latest_block"), &latestBlockIdString);
    if(latestBlockIdString.length() == 0){
        db->Get(leveldb::ReadOptions(), "latest_block_id", &latestBlockIdString);
    }
    
    //delete db;
    
    long result = -1;
    //std::stol(firstBlockId);
    if(latestBlockIdString.length() > 0 && parseLongValue(latestBlockIdString, result) == false){
        result = -1;
    }
    
    //std::cout << " z \n ";
    return result;
}

std::string CBlockDB::getLatestBlockHash(){
    leveldb::DB* db = getDatabase();
    if(!db){
        return "";
    }
    std::string latestHash;
    db->Get(leveldb::ReadOptions(), chainMetaKey("latest_hash"), &latestHash);
    if(latestHash.length() > 0){
        return latestHash;
    }
    long latestBlockId = getLatestBlockId();
    if(latestBlockId > 0){
        CFunctions::block_structure latestBlock = getBlock(latestBlockId);
        return latestBlock.hash;
    }
    return "";
}

int CBlockDB::getSchemaVersion(){
    leveldb::DB* db = getDatabase();
    if(!db){
        return 0;
    }
    long version = 0;
    if(readLongKey(db, chainMetaKey("schema_version"), version)){
        return (int)version;
    }
    return 0;
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

    leveldb::DB* db = getDatabase();
    if(db){
        db->Put(leveldb::WriteOptions(), chainMetaKey("connected_latest"), boost::lexical_cast<std::string>(latestConnectedBlockId));
    }

    return latestConnectedBlockId;
}

long CBlockDB::rebuildBestChainIndex(){
    std::lock_guard<std::mutex> rebuildLock(g_bestChainRebuildMutex);

    long firstBlockId = getFirstBlockId();
    if(firstBlockId < 0){
        return -1;
    }
    leveldb::DB* db = getDatabase();
    if(!db){
        return -1;
    }

    long previousTipId = getLatestBlockId();
    CFunctions::block_structure previousTip;
    previousTip.number = -1;
    if(previousTipId > 0){
        previousTip = getBlock(previousTipId);
    }

    long checkpointFastTip = -1;
    if(tryValidatedCheckpointFastPath(*this, db, checkpointFastTip)){
        return checkpointFastTip;
    }

    std::vector<CFunctions::block_structure> blocks = getStoredBlocks();
    if(blocks.size() == 0){
        return -1;
    }
    std::map<std::string, CFunctions::block_structure> blockByHash;
    std::map<std::string, std::vector<CFunctions::block_structure> > childrenByParentHash;
    std::vector<CFunctions::block_structure> genesisCandidates;
    for(int i = 0; i < blocks.size(); i++){
        CFunctions::block_structure block = blocks.at(i);
        if(block.hash.length() == 0){
            continue;
        }
        blockByHash[block.hash] = block;
        if(block.number == firstBlockId && block.previous_block_id <= 0){
            genesisCandidates.push_back(block);
        }
    }

    for(int i = 0; i < blocks.size(); i++){
        CFunctions::block_structure block = blocks.at(i);
        if(block.hash.length() == 0 || block.previous_block_hash.length() == 0){
            continue;
        }
        std::map<std::string, CFunctions::block_structure>::iterator parent = blockByHash.find(block.previous_block_hash);
        if(parent != blockByHash.end() && childConnectsToParent(block, parent->second)){
            childrenByParentHash[block.previous_block_hash].push_back(block);
        }
    }

    if(genesisCandidates.size() == 0){
        CFunctions::block_structure firstBlock = getBlock(firstBlockId);
        if(firstBlock.number > 0){
            genesisCandidates.push_back(firstBlock);
            blockByHash[firstBlock.hash] = firstBlock;
        }
    }

    std::sort(genesisCandidates.begin(), genesisCandidates.end(),
        [](const CFunctions::block_structure& a, const CFunctions::block_structure& b){
            if(a.number != b.number){
                return a.number < b.number;
            }
            return a.hash.compare(b.hash) < 0;
        });

    std::vector<CFunctions::block_structure> orderedBlocks;
    for(int i = 0; i < blocks.size(); i++){
        if(blocks.at(i).hash.length() > 0){
            orderedBlocks.push_back(blocks.at(i));
        }
    }
    std::sort(orderedBlocks.begin(), orderedBlocks.end(),
        [](const CFunctions::block_structure& a, const CFunctions::block_structure& b){
            if(a.number != b.number){
                return a.number > b.number;
            }
            return a.hash.compare(b.hash) < 0;
        });

    std::map<std::string, long> bestLengthByHash;
    std::map<std::string, long> bestTipByHash;
    std::map<std::string, std::string> bestChildByHash;

    for(int i = 0; i < orderedBlocks.size(); ++i){
        CFunctions::block_structure block = orderedBlocks.at(i);
        CFunctions::block_structure bestChild;
        bestChild.number = -1;

        std::map<std::string, std::vector<CFunctions::block_structure> >::const_iterator childrenIt = childrenByParentHash.find(block.hash);
        if(childrenIt != childrenByParentHash.end()){
            std::vector<CFunctions::block_structure> children = childrenIt->second;
            std::sort(children.begin(), children.end(),
                [](const CFunctions::block_structure& a, const CFunctions::block_structure& b){
                    if(a.number != b.number){
                        return a.number > b.number;
                    }
                    return a.hash.compare(b.hash) < 0;
                });
            for(int childIndex = 0; childIndex < children.size(); ++childIndex){
                CFunctions::block_structure child = children.at(childIndex);
                if(child.hash.length() == 0 || child.hash.compare(block.hash) == 0){
                    continue;
                }
                if(preferChainChild(child, bestChild, bestLengthByHash, bestTipByHash)){
                    bestChild = child;
                }
            }
        }

        long bestLength = 1;
        long bestTip = block.number;
        if(bestChild.number > 0 && bestChild.hash.length() > 0){
            std::map<std::string, long>::const_iterator lengthIt = bestLengthByHash.find(bestChild.hash);
            if(lengthIt != bestLengthByHash.end()){
                bestLength = lengthIt->second + 1;
            } else {
                bestLength = 2;
            }

            std::map<std::string, long>::const_iterator tipIt = bestTipByHash.find(bestChild.hash);
            if(tipIt != bestTipByHash.end()){
                bestTip = tipIt->second;
            } else {
                bestTip = bestChild.number;
            }

            bestChildByHash[block.hash] = bestChild.hash;
        }
        bestLengthByHash[block.hash] = bestLength;
        bestTipByHash[block.hash] = bestTip;
    }

    CFunctions::block_structure bestGenesis;
    bestGenesis.number = -1;
    for(int i = 0; i < genesisCandidates.size(); ++i){
        CFunctions::block_structure candidate = genesisCandidates.at(i);
        if(preferChainChild(candidate, bestGenesis, bestLengthByHash, bestTipByHash)){
            bestGenesis = candidate;
        }
    }

    std::vector<CFunctions::block_structure> bestChain;
    std::set<std::string> visitedHashes;
    CFunctions::block_structure current = bestGenesis;
    while(current.number > 0 && current.hash.length() > 0){
        if(visitedHashes.find(current.hash) != visitedHashes.end()){
            break;
        }
        bestChain.push_back(current);
        visitedHashes.insert(current.hash);
        if(bestChain.size() >= MAX_REBUILD_CHAIN_DEPTH){
            break;
        }

        std::map<std::string, std::string>::const_iterator nextIt = bestChildByHash.find(current.hash);
        if(nextIt == bestChildByHash.end()){
            break;
        }
        std::map<std::string, CFunctions::block_structure>::const_iterator blockIt = blockByHash.find(nextIt->second);
        if(blockIt == blockByHash.end()){
            break;
        }
        current = blockIt->second;
    }

    while(bestChain.size() > 0){
        std::string reason;
        if(CChainValidator::validateConnectedChain(bestChain, reason)){
            break;
        }
        bestChain.pop_back();
    }
    if(bestChain.size() == 0){
        return -1;
    }

    long latestBestBlockId = bestChain.back().number;
    CFunctions::block_structure newTip = bestChain.back();

    leveldb::WriteOptions writeOptions;
    std::vector<std::string> nextIndexKeys;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for(it->SeekToFirst(); it->Valid(); it->Next()){
        std::string key = it->key().ToString();
        if(boost::algorithm::starts_with(key, "next_block_") ||
           boost::algorithm::starts_with(key, "next:")){
            nextIndexKeys.push_back(key);
        }
    }
    delete it;

    for(int i = 0; i < nextIndexKeys.size(); i++){
        db->Delete(writeOptions, nextIndexKeys.at(i));
    }

    for(int i = 0; i < bestChain.size(); ++i){
        CFunctions functions;
        CFunctions::block_structure canonicalBlock = bestChain.at(i);
        ostringstream blockKeyStream;
        blockKeyStream << "b_" << canonicalBlock.number;
        db->Put(writeOptions, canonicalHashKey(canonicalBlock.number), canonicalBlock.hash);
        db->Put(writeOptions, blockHashKey(canonicalBlock.hash), functions.blockJSON(canonicalBlock));
        db->Put(writeOptions, blockKeyStream.str(), functions.blockJSON(canonicalBlock));

        if(i + 1 < bestChain.size()){
            CFunctions::block_structure childBlock = bestChain.at(i + 1);
            ostringstream keyStream;
            keyStream << "next_block_" << canonicalBlock.number;
            ostringstream valueStream;
            valueStream << childBlock.number;
            db->Put(writeOptions, keyStream.str(), valueStream.str());
            db->Put(writeOptions, nextHashKey(canonicalBlock.hash), childBlock.hash);
        }
    }

    setLatestBlockId(latestBestBlockId);
    db->Put(writeOptions, chainMetaKey("connected_latest"), boost::lexical_cast<std::string>(latestBestBlockId));
    maybeAdvanceValidatedCheckpoint(db, newTip);
    if(previousTip.number > 0 &&
       previousTip.hash.length() > 0 &&
       newTip.number > 0 &&
       newTip.hash.length() > 0 &&
       previousTip.hash.compare(newTip.hash) != 0){
        std::string previousBlock = boost::lexical_cast<std::string>(previousTip.number);
        std::string newBlock = boost::lexical_cast<std::string>(newTip.number);
        db->Put(writeOptions, reorgMetaKey("previous_block"), previousBlock);
        db->Put(writeOptions, reorgMetaKey("previous_hash"), previousTip.hash);
        db->Put(writeOptions, reorgMetaKey("new_block"), newBlock);
        db->Put(writeOptions, reorgMetaKey("new_hash"), newTip.hash);
        db->Put(writeOptions, reorgMetaKey("time"), boost::lexical_cast<std::string>(std::time(0)));
        db->Put(writeOptions, reorgMetaKey("reason"), "best chain index rebuilt");

        CFileLogger log;
        log.log("Chain reorg: " + previousBlock + " " + previousTip.hash + " -> " + newBlock + " " + newTip.hash + "\n");
    }
    return latestBestBlockId;
}

long CBlockDB::getForkVariantCount(){
    leveldb::DB* db = getDatabase();
    if(!db){
        return 0;
    }

    ForkVariantStats stats = scanForkVariantStats(db);
    leveldb::WriteOptions writeOptions;
    db->Put(writeOptions, forkVariantCountKey(), boost::lexical_cast<std::string>(stats.count));
    if(stats.latest_block > 0){
        db->Put(writeOptions, forkVariantLatestBlockKey(), boost::lexical_cast<std::string>(stats.latest_block));
    }
    return stats.count;
}

std::vector<CBlockDB::fork_variant> CBlockDB::getForkVariants(int limit){
    std::vector<fork_variant> variants;
    std::vector<CFunctions::block_structure> blocks = getStoredBlocks();
    std::map<long, long> variantsByBlock;
    for(int i = 0; i < blocks.size(); i++){
        CFunctions::block_structure block = blocks.at(i);
        if(block.number > 0 && block.hash.length() > 0){
            variantsByBlock[block.number]++;
        }
    }

    for(int i = 0; i < blocks.size(); i++){
        CFunctions::block_structure block = blocks.at(i);
        if(block.number <= 0 || block.hash.length() == 0 || variantsByBlock[block.number] <= 1){
            continue;
        }
        CFunctions::block_structure canonicalBlock = getBlock(block.number);
        fork_variant variant;
        variant.block_number = block.number;
        variant.hash = block.hash;
        variant.previous_hash = block.previous_block_hash;
        variant.creator_key = block.creator_key;
        variant.canonical = canonicalBlock.hash.length() > 0 && canonicalBlock.hash.compare(block.hash) == 0;
        variants.push_back(variant);
    }

    std::sort(variants.begin(), variants.end(),
        [](const CBlockDB::fork_variant& a, const CBlockDB::fork_variant& b){
            if(a.block_number != b.block_number){
                return a.block_number > b.block_number;
            }
            if(a.canonical != b.canonical){
                return a.canonical;
            }
            return a.hash.compare(b.hash) < 0;
        });

    if(limit > 0 && variants.size() > limit){
        variants.resize(limit);
    }
    return variants;
}

CBlockDB::reorg_info CBlockDB::getLastReorgInfo(){
    reorg_info info;
    info.previous_block = -1;
    info.new_block = -1;
    info.time = 0;

    leveldb::DB* db = getDatabase();
    if(!db){
        return info;
    }

    std::string previousBlock;
    std::string newBlock;
    std::string time;
    db->Get(leveldb::ReadOptions(), reorgMetaKey("previous_block"), &previousBlock);
    db->Get(leveldb::ReadOptions(), reorgMetaKey("previous_hash"), &info.previous_hash);
    db->Get(leveldb::ReadOptions(), reorgMetaKey("new_block"), &newBlock);
    db->Get(leveldb::ReadOptions(), reorgMetaKey("new_hash"), &info.new_hash);
    db->Get(leveldb::ReadOptions(), reorgMetaKey("time"), &time);
    db->Get(leveldb::ReadOptions(), reorgMetaKey("reason"), &info.reason);

    if(previousBlock.length() > 0){
        info.previous_block = boost::lexical_cast<long>(previousBlock);
    }
    if(newBlock.length() > 0){
        info.new_block = boost::lexical_cast<long>(newBlock);
    }
    if(time.length() > 0){
        info.time = boost::lexical_cast<long>(time);
    }
    return info;
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
        parseLongValue(nextBlockIdString, result);
    }
    if(result <= 0){
        CFunctions::block_structure previousBlock = getBlock(previousBlockId);
        if(previousBlock.hash.length() > 0){
            std::string nextHash;
            db->Get(leveldb::ReadOptions(), nextHashKey(previousBlock.hash), &nextHash);
            if(nextHash.length() > 0){
                CFunctions::block_structure nextBlock = getBlockByHash(nextHash);
                if(childConnectsToParent(nextBlock, previousBlock)){
                    result = nextBlock.number;
                }
            }
        }
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
CFunctions::block_structure CBlockDB::getNextBlockByHash(CFunctions::block_structure block){
    CFunctions::block_structure nextBlock;
    nextBlock.number = -1;
    if(block.number <= 0 || block.hash.length() == 0){
        return nextBlock;
    }

    leveldb::DB* db = getDatabase();
    if(db){
        std::string nextHash;
        db->Get(leveldb::ReadOptions(), nextHashKey(block.hash), &nextHash);
        if(nextHash.length() > 0){
            nextBlock = getBlockByHash(nextHash);
            if(childConnectsToParent(nextBlock, block)){
                return nextBlock;
            }
        }
    }

    long legacyNextId = -1;
    std::string legacyNextIdString;
    if(db){
        std::stringstream keyStream;
        keyStream << "next_block_" << block.number;
        db->Get(leveldb::ReadOptions(), keyStream.str(), &legacyNextIdString);
        parseLongValue(legacyNextIdString, legacyNextId);
    }
    if(legacyNextId > 0){
        nextBlock = getBlock(legacyNextId);
        if(nextBlock.number > 0 && childConnectsToParent(nextBlock, block)){
            return nextBlock;
        }
    }

    std::vector<CFunctions::block_structure> blocks = getStoredBlocks();
    for(int i = 0; i < blocks.size(); i++){
        CFunctions::block_structure candidate = blocks.at(i);
        if(childConnectsToParent(candidate, block) && preferBlockVariant(candidate, nextBlock)){
            nextBlock = candidate;
        }
    }
    return nextBlock;
}

CFunctions::block_structure CBlockDB::getNextBlock(CFunctions::block_structure block){
    CFunctions::block_structure nextBlock = getNextBlockByHash(block);
    if(nextBlock.number > 0){
        return nextBlock;
    }
    long nextId = getNextBlockId(block.number);
    nextBlock = getBlock(nextId);
    if(nextBlock.number > 0 && childConnectsToParent(nextBlock, block)){
        return nextBlock;
    }
    nextBlock.number = -1;
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
    std::vector<std::string> keys;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        keys.push_back(it->key().ToString());
    }
    if (false == it->status().ok())
    {
        cerr << "An error was found during the scan" << endl;
        cerr << it->status().ToString() << endl;
    }
    delete it;

    for(int i = 0; i < keys.size(); i++){
        db->Delete(writeOptions, keys.at(i));
        cout << "Delete: " << keys.at(i) << "\n";
    }
    //delete db;
}
