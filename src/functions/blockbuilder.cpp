// Copyright (c) 2016 2017 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
//  blockbuilder.cpp
//  
//
//  Created by Jon on 2018-02-13.
//

#include "functions/blockbuilder.hpp"
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "network/relayclient.h"
#include "network/localpeerclient.h"
#include "networktime.h"
#include "functions/selector.h"
#include "functions/chain.h"
#include "functions/ledgerstate.h"
#include "wallet.h"
#include "blockdb.h"
#include "userdb.h"
#include "log.h"
#include "networkconfig.h"
#include <fstream>
#include <map>
#include <set>

volatile bool isBuildingBlocks = true;

namespace {

bool hasPendingHeartbeat(CFunctions& functions, const std::string& publicKey)
{
    std::vector<CFunctions::record_structure> records = functions.peekQueueRecords();
    for(int i = 0; i < records.size(); i++){
        CFunctions::record_structure record = records.at(i);
        if(record.transaction_type == CFunctions::HEART_BEAT &&
           record.sender_public_key.compare(publicKey) == 0){
            return true;
        }
    }
    return false;
}

bool recordExistsInAcceptedChain(CBlockDB& blockDB, const std::string& recordHash)
{
    if(recordHash.length() == 0){
        return false;
    }

    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(firstBlockId < 0 || latestBlockId < 0){
        return false;
    }

    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            if(block.records.at(i).hash.compare(recordHash) == 0){
                return true;
            }
        }

        if(block.number == latestBlockId){
            break;
        }
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
        if(nextBlock.number <= 0 || nextBlock.number == block.number){
            break;
        }
        block = nextBlock;
        guard++;
    }
    return false;
}

void replacePendingQueueRecords(CFunctions& functions, const std::vector<CFunctions::record_structure>& records)
{
    functions.parseQueueRecords();
    for(int i = 0; i < records.size(); i++){
        functions.addToQueue(records.at(i));
    }
}

void pruneAcceptedQueueRecords(CFunctions& functions, CBlockDB& blockDB)
{
    std::vector<CFunctions::record_structure> records = functions.peekQueueRecords();
    std::vector<CFunctions::record_structure> keptRecords;
    std::set<std::string> seenHashes;
    bool changed = false;

    for(int i = 0; i < records.size(); i++){
        CFunctions::record_structure record = records.at(i);
        if(record.hash.length() > 0){
            if(seenHashes.find(record.hash) != seenHashes.end()){
                changed = true;
                continue;
            }
            seenHashes.insert(record.hash);
            if(recordExistsInAcceptedChain(blockDB, record.hash)){
                changed = true;
                continue;
            }
        }
        keptRecords.push_back(record);
    }

    if(changed){
        replacePendingQueueRecords(functions, keptRecords);
    }
}

void rebroadcastPendingQueueRecords(CFunctions& functions, CRelayClient& relayClient, CBlockDB& blockDB)
{
    pruneAcceptedQueueRecords(functions, blockDB);

    std::vector<CFunctions::record_structure> records = functions.peekQueueRecords();
    for(int i = 0; i < records.size(); i++){
        CFunctions::record_structure record = records.at(i);
        if(record.hash.length() == 0 || recordExistsInAcceptedChain(blockDB, record.hash)){
            continue;
        }
        // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
        // relayClient.sendRecord(record);
        CLocalPeerClient::broadcastRecord(record);
    }
}

bool includePendingTransfer(
    const CFunctions::record_structure& record,
    std::map<std::string, double>& pendingBalances,
    std::map<std::string, long>& pendingNonces)
{
    if(record.transaction_type != CFunctions::TRANSFER_CURRENCY){
        return true;
    }
    if(record.nonce <= 0){
        return false;
    }

    long expectedNonce = pendingNonces[record.sender_public_key] + 1;
    if(record.nonce != expectedNonce){
        return false;
    }

    double debit = record.sender_public_key.compare(record.recipient_public_key) == 0 ? record.fee : record.amount + record.fee;
    if(pendingBalances[record.sender_public_key] + 0.000001 < debit){
        return false;
    }

    pendingBalances[record.recipient_public_key] += record.amount;
    pendingBalances[record.sender_public_key] -= record.amount + record.fee;
    pendingNonces[record.sender_public_key] = record.nonce;
    return true;
}

void queueHeartbeatIfDue(
    CFunctions& functions,
    CRelayClient& relayClient,
    CECDSACrypto& ecdsa,
    const std::string& privateKey,
    const std::string& publicKey
){
    if(functions.joined == false || functions.heartbeat_renewal_due == false){
        return;
    }
    if(hasPendingHeartbeat(functions, publicKey)){
        return;
    }

    CFunctions::record_structure heartbeatRecord;
    heartbeatRecord.network = "main";
    CNetworkTime netTime;
    std::stringstream ss;
    ss << netTime.getEpoch();
    heartbeatRecord.time = ss.str();
    heartbeatRecord.transaction_type = CFunctions::HEART_BEAT;
    heartbeatRecord.amount = 0.0;
    heartbeatRecord.fee = 0.0;
    heartbeatRecord.sender_public_key = publicKey;
    heartbeatRecord.recipient_public_key = "";
    heartbeatRecord.hash = functions.getRecordHash(heartbeatRecord);
    std::string signature = "";
    ecdsa.SignMessage(privateKey, heartbeatRecord.hash, signature);
    heartbeatRecord.signature = signature;

    if(functions.addToQueue(heartbeatRecord)){
        // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
        // relayClient.sendRecord(heartbeatRecord);
        CLocalPeerClient::broadcastRecord(heartbeatRecord);
    }
}

}

/**
 * blockBuilderThread TODO: Move this to blockbuilder.cpp
 *
 * Description:
 * std::thread blockThread (blockBuilderThread);
 */
void CBlockBuilder::blockBuilderThread(int argc, char* argv[]){
    //std::cout << "CBlockBuilder 1\n";
    CECDSACrypto ecdsa;
    CFunctions functions;
    CBlockDB blockDB;
    CUserDB userDB;
    CRelayClient relayClient;
    CSelector selector;
    selector.syncronizeTime();
    CFileLogger log;
    //CChain chain;   // depricate
    //CFileLogger log;
    //CBlockDB blockDB;
    // Check block chain for latest block information.
    // TODO...
    
    // Syncronize local time with server
    // get local time and server time and calculate difference to add to local time.
    // ....
    
    // Get current user keys
    std::string privateKey;
    std::string publicKey;
    CWallet wallet;
    bool e = wallet.fileExists("wallet.dat");
    //printf(" wallet exists: %d  \n", e);
    if(e == 0){
        //printf("No wallet found. Creating a new one...\n");
    } else {
        // Load wallet
        wallet.readCreatorAccount(privateKey, publicKey);
        //std::cout << "  private  " << privateKey << "\n  public " << publicKey << "\n " << std::endl;
    }
    
    //functions.parseBlockFile(publicKey, false); // depricate
    functions.scanChain(publicKey, false);
    
    // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
    // relayClient.getNewNetworkPeer(publicKey);
    
    bool networkGenesis = false;
    std::string networkName = "main";
    if(argc >= 3 && strcmp(argv[1], "genesis") == 0){ // && argv[1] == "genesis"
        networkGenesis = true;
        networkName = argv[2];
        //std::cout << " 1 " << argv[1] << " " << argv[2];
    }
    
    long timeBlock = selector.getCurrentTimeBlock();
    
    if(networkGenesis){
        std::cout << "Creating genesis block for a new network named " << networkName << std::endl;
        
        // Clear past data (for given network name)
        blockDB.DeleteAll();
        
        //log.log("Creating genesis block.\n");
        //log.clearLog();
        
        CFunctions::record_structure joinRecord;
        joinRecord.network = networkName;
        CNetworkTime netTime;
        std::stringstream ss;
        ss << netTime.getEpoch();
        std::string ts = ss.str();
        joinRecord.time = ts;
        CFunctions::transaction_types joinType = CFunctions::JOIN_NETWORK;
        joinRecord.transaction_type = joinType;
        joinRecord.amount = 0.0;
        joinRecord.fee = 0;
        joinRecord.sender_public_key = publicKey;
        joinRecord.recipient_public_key = "";
        joinRecord.hash = functions.getRecordHash(joinRecord);
        std::string signature = "";
        ecdsa.SignMessage(privateKey, joinRecord.hash, signature);
        joinRecord.signature = signature;
        
        CFunctions::record_structure blockRewardRecord;
        blockRewardRecord.network = networkName;
        blockRewardRecord.time = ts;
        CFunctions::transaction_types transaction_type = CFunctions::ISSUE_CURRENCY;
        blockRewardRecord.transaction_type = transaction_type;
        blockRewardRecord.amount = 1.0;
        blockRewardRecord.sender_public_key = publicKey;
        blockRewardRecord.recipient_public_key = publicKey;
        blockRewardRecord.hash = functions.getRecordHash(blockRewardRecord);
        ecdsa.SignMessage(privateKey, blockRewardRecord.hash, signature);
        blockRewardRecord.signature = signature;
        
        CFunctions::block_structure block;
        block.creator_key = publicKey;
        block.network = networkName;
        block.records.push_back(joinRecord);
        block.records.push_back(blockRewardRecord);
        
        //time_t t = time(0);
        //std::string block_time = std::asctime(std::localtime(&t));
        
        block.number = selector.getCurrentTimeBlock();
        block.previous_block_id = -1;
        
        //time_t  epoch;
        //time(&epoch);
        //std::stringstream ss;
        //ss << epoch;
        //std::string ts = ss.str();
        block.time = ts;
        
        block.records_merkle_root = functions.getRecordsMerkleRoot(block.records);
        block.hash = functions.getBlockHash(block);
        ecdsa.SignMessage(privateKey, block.hash, signature);
        block.signature = signature;
        
        //chain.setFirstBlock(block); // depricate
        //functions.addToBlockFile(block);
        
        //blockDB.addFirstBlock(block); // depricate
        CNetworkConfig config = CNetworkConfig::load();
        config.network = networkName;
        config.genesisBlock = block.number;
        config.genesisHash = block.hash;
        config.strictGenesis = true;
        config.save();

        blockDB.AddBlock(block);
        blockDB.setFirstBlockId(block.number);
        
        // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
        // relayClient.sendBlock(block);
        CLocalPeerClient::broadcastBlock(block);
        CLocalPeerClient::broadcastHandoff(block, privateKey);
        
        // Wait until the block period is over
        long currTimeBlock = selector.getCurrentTimeBlock();
        while( currTimeBlock == timeBlock ){
            usleep(1000000);
            currTimeBlock = selector.getCurrentTimeBlock();
            //std::cout << "." << std::endl;
        }
    }
    
    //std::cout << "CBlockBuilder 2\n";
    
    // Does a blockfile exist? if networkGenesis==false and there is no block file we wait until the network syncs before wrting to the blockfile.
    CFunctions::block_structure previous_block;
    
    long firstBlockId = blockDB.getFirstBlockId();
    
    //std::cout << "CBlockBuilder 3\n";
    
    // syncronize chain
    if(networkGenesis == false && firstBlockId == -1){ // && firstBlockId == -1
        std::cout << "Synchronizing blockchain." << std::endl; // Downloading
        
        // Expected latest block number.
        bool localPeerMode = CLocalPeerClient::getPeers().size() > 0;
        if(localPeerMode){
            std::cout << " Latest local block: " << blockDB.getLatestBlockId() << " peer latest block: " << CLocalPeerClient::getBestPeerLatestBlockId() << std::endl;
        } else {
            std::cout << " Latest local block: " << blockDB.getLatestBlockId() << std::endl;
            // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
            // relayClient.sendRequestBlocks(-1);
        }
        //log.log("Request the beginning of the blockchain from our peer nodes. \n");
        
        // ISSUE: CRelayClient::receiveBlocks() will receive this block but not know it is the genesis (-1) block.
        // Solution: just use the block.previous_block_id member. Only the genesis block won't have a value.
        
        //std::cout << "-" << std::endl;
        int progressPos = 0;
        //long latestBlockId = blockDB.getLatestBlockId();
        // functions.IsChainUpToDate() == false
        while( functions.IsChainUpToDate() == false && isBuildingBlocks){
            long targetBlock = localPeerMode ? CLocalPeerClient::getBestPeerLatestBlockId() : -1;
            std::string targetLabel = "waiting for genesis";
            if(targetBlock > -1){
                std::stringstream targetStream;
                targetStream << targetBlock;
                targetLabel = targetStream.str();
            }
            if(progressPos == 0){
                std::cout << "\rSynchronizing: " << "|" << " " <<
                    blockDB.getLatestBlockId() << "-" << targetLabel << std::flush; progressPos++;
            } else if(progressPos == 1){
                std::cout << "\rSynchronizing: " << "/" << " " <<
                    blockDB.getLatestBlockId() << "-" << targetLabel << std::flush; progressPos++;
            } else if(progressPos == 2){
                std::cout << "\rSynchronizing: " << "-" << " " <<
                    blockDB.getLatestBlockId() << "-" << targetLabel << std::flush; progressPos++;
            } else if(progressPos == 3){
                std::cout << "\rSynchronizing: " << "-" << " " <<
                    blockDB.getLatestBlockId() << "-" << targetLabel << std::flush; progressPos = 0;
            }
            
            // if no progress, send request again.
            
            usleep(1000000);
            
            if(localPeerMode){
                std::vector<std::string> localPeers = CLocalPeerClient::getPeers();
                for(int i = 0; i < localPeers.size(); i++){
                    CLocalPeerClient::syncFromPeer(localPeers.at(i));
                }
            } else {
                // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
                // relayClient.sendRequestBlocks(blockDB.getLatestBlockId());
            }
        }
        //std::cout << "Chain is up to date. " << std::endl;
    }
    
    long lastLocalBuiltBlockId = -1;
    long lastPendingRecordRebroadcastEpoch = 0;
    
    int blockNumber = functions.latest_block.number + 1;
    //long timeBlock = 0;
    while(isBuildingBlocks){
        wallet.readCreatorAccount(privateKey, publicKey);
        //functions.parseBlockFile(publicKey, false); // depricate
        functions.scanChain(publicKey, false); // ??? check this
        queueHeartbeatIfDue(functions, relayClient, ecdsa, privateKey, publicKey);
        CNetworkTime rebroadcastTime;
        long rebroadcastEpoch = rebroadcastTime.getLocalEpoch();
        if(lastPendingRecordRebroadcastEpoch == 0 || rebroadcastEpoch - lastPendingRecordRebroadcastEpoch >= 30){
            rebroadcastPendingQueueRecords(functions, relayClient, blockDB);
            lastPendingRecordRebroadcastEpoch = rebroadcastEpoch;
        }
        
        timeBlock = selector.getCurrentTimeBlock();
        //std::cout << "here " << std::endl;
        
        previous_block = blockDB.getBlock(blockDB.getLatestBlockId());

        // Is it the current user's turn to generate a block from the known canonical parent?
        bool build_block = false;
        if(previous_block.number > 0 && previous_block.number < timeBlock){
            long selectionBoundary = CSelector::getSelectionBoundaryBlock(timeBlock, blockDB.getFirstBlockId());
            CLedgerState::state selectionState = CLedgerState::build(blockDB, "", selectionBoundary);
            std::vector<std::string> activeMemberKeys = CLedgerState::activeMemberKeysAt(selectionState, selectionState.latest_block_id);
            std::string selectedCreator = CSelector::getSelectedUserForBlock(timeBlock, selectionState.latest_block.hash, activeMemberKeys);
            build_block = selectedCreator.compare(publicKey) == 0;
        }
        // Can't build block unless a member of the block.
        if(functions.joined == false){
            build_block = false;
        }
        if(CLocalPeerClient::getPeers().size() > 0 && !CLocalPeerClient::isSyncedWithPeers()){
            build_block = false;
        }
        // If latest block is not up to date, don't build new block
        // Issue, this will fail if there is a gap in the chain. If a node doesn't generate a block.
        //long currBlock = selector.getCurrentTimeBlock(); // timeBlock
        //if(blockDB.getLatestBlockId() < currBlock - 1){
            //build_block = false;
        //}
        
        //std::cout << "here 2 " << std::endl;
        
        //time_t t = time(0);
        //std::string block_time = std::asctime(std::localtime(&t));
        
        //std::cout << " building blocks " << std::endl;
        
        if(build_block){
            log.log(" build yes \n");
        } else {
            log.log(" build no \n");
        }
        
        if(build_block){
            log.log("Generate new block\n");
            
            // While time remaining in block
            if(!isBuildingBlocks){
                return;
            }
            
            CNetworkTime netTime;
            std::stringstream ss;
            ss << netTime.getEpoch();
            std::string ts = ss.str();
            
            CFunctions::record_structure blockRewardRecord;
            blockRewardRecord.network = networkName;
            blockRewardRecord.time = ts;
            blockRewardRecord.transaction_type = CFunctions::ISSUE_CURRENCY;
            blockRewardRecord.amount = 1.0;
            blockRewardRecord.fee = 0;
            blockRewardRecord.sender_public_key = publicKey;
            blockRewardRecord.recipient_public_key = publicKey;
            blockRewardRecord.hash = functions.getRecordHash(blockRewardRecord);
            std::string signature = "";
            ecdsa.SignMessage(privateKey, blockRewardRecord.hash, signature);
            blockRewardRecord.signature = signature;
            
            CFunctions::block_structure block;
            block.creator_key = publicKey;
            block.records.push_back(blockRewardRecord);
            std::set<std::string> includedRecordHashes;
            if(blockRewardRecord.hash.length() > 0){
                includedRecordHashes.insert(blockRewardRecord.hash);
            }
            
            block.number = selector.getCurrentTimeBlock(); //  blockNumber++;
            block.time = ts;
            
            //std::cout << " XXX \n";
            long latestBlockId = blockDB.getLatestBlockId();
            if(latestBlockId == -1){
                std::cout << "ERROR: Block generation without previous block assigned. \n";
            }
            block.previous_block_id = latestBlockId;

            CLedgerState::state pendingState = CLedgerState::build(blockDB, "", latestBlockId);
            std::map<std::string, double> pendingBalances = pendingState.balances;
            std::map<std::string, long> pendingNonces = pendingState.nonces;
            pendingBalances[publicKey] += blockRewardRecord.amount;
            
            // Add records from queue...
            std::vector< CFunctions::record_structure > records = functions.parseQueueRecords();
            for(int i = 0; i < records.size(); i++){
                CFunctions::record_structure queuedRecord = records[i];
                if(includePendingTransfer(queuedRecord, pendingBalances, pendingNonces) == false){
                    continue;
                }
                if(queuedRecord.hash.length() == 0 || includedRecordHashes.find(queuedRecord.hash) == includedRecordHashes.end()){
                    block.records.push_back(queuedRecord);
                    if(queuedRecord.hash.length() > 0){
                        includedRecordHashes.insert(queuedRecord.hash);
                    }
                }
                
                // TODO: watch time so that there is enough to broadcast the block in order to have it accepted.
            }
            
            block.previous_block_hash = previous_block.hash;
            block.records_merkle_root = functions.getRecordsMerkleRoot(block.records);
            block.hash = functions.getBlockHash(block);
            ecdsa.SignMessage(privateKey, block.hash, signature);
            block.signature = signature;
            
            if(block.number != lastLocalBuiltBlockId){
                blockDB.AddBlock(block);
                // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
                // relayClient.sendBlock(block);
                CLocalPeerClient::broadcastBlock(block);
                CLocalPeerClient::broadcastHandoff(block, privateKey);
            }
            lastLocalBuiltBlockId = block.number;
            
            //previous_block = block; // temp
            
            // Wait until the block period is over
            long currTimeBlock = selector.getCurrentTimeBlock();
            while( currTimeBlock == timeBlock ){
                usleep(1000000); // One second
                //usleep(300000);
                
                currTimeBlock = selector.getCurrentTimeBlock();
                //std::cout << ".";
            }
            //std::cout << "block done" << std::endl;
            
        } else { // Not building this block
            //log.log(".\n");
            
            bool needsLocalPeerSync = CLocalPeerClient::getPeers().size() > 0 && !CLocalPeerClient::isSyncedWithPeers();
            if( functions.IsChainUpToDate() == false || needsLocalPeerSync ){
            
                firstBlockId = blockDB.getFirstBlockId();
                if(firstBlockId == -1 && CLocalPeerClient::getPeers().size() == 0){
                    // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
                    // relayClient.sendRequestBlocks(-1);
                }
                
                // Download recent blocks to keep the local chain up to date.
                long currBlock = selector.getCurrentTimeBlock();
                //if(blockDB.getLatestBlockId() < currBlock - 0 && isBuildingBlocks){
                    log.log("Block builder. Currently behind on chain. Requesting blocks from the network...\n");
                    
                    //std::cout << "Syncronizing Blockchain." << std::endl;
                    if(CLocalPeerClient::getPeers().size() > 0){
                        std::vector<std::string> localPeers = CLocalPeerClient::getPeers();
                        for(int i = 0; i < localPeers.size(); i++){
                            CLocalPeerClient::syncFromPeer(localPeers.at(i));
                        }
                    } else {
                        // Legacy PHP relay disabled. Use CLocalPeerClient /api peer sync instead.
                        // relayClient.sendRequestBlocks(blockDB.getLatestBlockId());
                    }
                //}
                
            }
            
            usleep(1000000); //
            //usleep(100000);
            
            //std::cout << "wait" << std::endl;
        }
        
        if(!isBuildingBlocks){ // ??? why
            usleep(1000000);
        }
    }
}

void CBlockBuilder::stop() {
    if ( !isBuildingBlocks ) return;
    isBuildingBlocks = false;
    // Join thread
}
