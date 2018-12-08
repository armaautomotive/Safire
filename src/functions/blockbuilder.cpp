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
#include "functions/selector.h"
#include "functions/chain.h"
#include "wallet.h"
#include "blockdb.h"
#include "log.h"

volatile bool isBuildingBlocks = true;

/**
 * blockBuilderThread TODO: Move this to blockbuilder.cpp
 *
 * Description:
 * std::thread blockThread (blockBuilderThread);
 */
void CBlockBuilder::blockBuilderThread(int argc, char* argv[]){
    std::cout << "CBlockBuilder 1\n";
    CECDSACrypto ecdsa;
    CFunctions functions;
    CBlockDB blockDB;
    CRelayClient relayClient;
    CSelector selector;
    selector.syncronizeTime();
    CChain chain;   // depricate
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
        wallet.read(privateKey, publicKey);
        //std::cout << "  private  " << privateKey << "\n  public " << publicKey << "\n " << std::endl;
    }
    
    //functions.parseBlockFile(publicKey, false); // depricate
    functions.scanChain(publicKey, false);
    
    relayClient.getNewNetworkPeer(publicKey);
    
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
        time_t  timev;
        time(&timev);
        std::stringstream ss;
        ss << timev;
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
        
        // *** TESTING ONLY ***
        std::string fictionPrivateKey;
        std::string fictionPublicKey;
        std::string uncompressed;
        int r = ecdsa.RandomPrivateKey(fictionPrivateKey);
        r = ecdsa.GetPublicKey(fictionPrivateKey, uncompressed, fictionPublicKey);
        CFunctions::record_structure fictionJoinRecord;
        fictionJoinRecord.network = networkName;
        fictionJoinRecord.time = ts;
        fictionJoinRecord.transaction_type = joinType;
        fictionJoinRecord.amount = 0.0;
        fictionJoinRecord.fee = 0;
        fictionJoinRecord.sender_public_key = fictionPublicKey;
        fictionJoinRecord.recipient_public_key = "";
        fictionJoinRecord.hash = functions.getRecordHash(fictionJoinRecord);
        ecdsa.SignMessage(fictionPrivateKey, fictionJoinRecord.hash, signature);
        fictionJoinRecord.signature = signature;
        // *** END TESTING ***
        
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
        block.records.push_back(fictionJoinRecord);
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
        
        block.hash = functions.getBlockHash(block);
        ecdsa.SignMessage(privateKey, block.hash, signature);
        block.signature = signature;
        
        //chain.setFirstBlock(block); // depricate
        //functions.addToBlockFile(block);
        
        //blockDB.addFirstBlock(block); // depricate
        blockDB.AddBlock(block);
        blockDB.setFirstBlockId(block.number);
        
        relayClient.sendBlock(block);
        
        // Wait until the block period is over
        long currTimeBlock = selector.getCurrentTimeBlock();
        while( currTimeBlock == timeBlock ){
            usleep(1000000);
            currTimeBlock = selector.getCurrentTimeBlock();
            //std::cout << "." << std::endl;
        }
    }
    
    std::cout << "CBlockBuilder 2\n";
    
    // Does a blockfile exist? if networkGenesis==false and there is no block file we wait until the network syncs before wrting to the blockfile.
    CFunctions::block_structure previous_block;
    
    long firstBlockId = blockDB.getFirstBlockId();
    
    std::cout << "CBlockBuilder 3\n";
    
    // syncronize chain
    if(networkGenesis == false && firstBlockId == -1 ){ // chain.getFirstBlock() == -1
        //
        std::cout << "Syncronizing Blockchain." << std::endl;
        
        // Expected latest block number.
        long currBlock = selector.getCurrentTimeBlock();
        std::cout << "Current time: " << currBlock << " latest: " << chain.getLatestBlock() << std::endl;
        
        relayClient.sendRequestBlocks(-1); // Request the beginning of the blockchain from our peer nodes.
        //log.log("Request the beginning of the blockchain from our peer nodes. \n");
        
        
        //std::cout << "-" << std::endl;
        int progressPos = 0;
        while( chain.getLatestBlock() < currBlock - 1 && isBuildingBlocks ){
            if(progressPos == 0){
                std::cout << "\rSynchronizing: " << "|" << std::flush; progressPos++;
            } else if(progressPos == 1){
                std::cout << "\rSynchronizing: " << "/" << std::flush; progressPos++;
            } else if(progressPos == 2){
                std::cout << "\rSynchronizing: " << "-" << std::flush; progressPos++;
            } else if(progressPos == 3){
                std::cout << "\rSynchronizing: " << "-" << std::flush; progressPos = 0;
            }
            
            // if no progress, send request again.
            
            usleep(1000000);
            
            currBlock = selector.getCurrentTimeBlock();
            relayClient.sendRequestBlocks(chain.getLatestBlock());
            
        }
    }
    
    int blockNumber = functions.latest_block.number + 1;
    //long timeBlock = 0;
    while(isBuildingBlocks){
        //functions.parseBlockFile(publicKey, false); // depricate
        functions.scanChain(publicKey, false);
        
        timeBlock = selector.getCurrentTimeBlock();
        //std::cout << "here " << std::endl;
        
        // is it current users turn to generate a block.
        bool build_block = selector.isSelected(publicKey);
        if(functions.joined == false){ // Can't build block unless a member of the block.
            build_block = false;
        }
        //std::cout << "here 2 " << std::endl;
        
        //time_t t = time(0);
        //std::string block_time = std::asctime(std::localtime(&t));
        
        //std::cout << " building blocks " << std::endl;
        
        if(build_block){
            
            // While time remaining in block
            if(!isBuildingBlocks){
                return;
            }
            
            time_t  timev;
            time(&timev);
            std::stringstream ss;
            ss << timev;
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
            
            
            // ***
            // Example transaction from transaction queue file.
            // ***
            CFunctions::record_structure sendRecord;
            sendRecord.time = ts;
            sendRecord.transaction_type = CFunctions::TRANSFER_CURRENCY;
            sendRecord.amount = 0.0123;
            sendRecord.fee = 0.001;
            sendRecord.sender_public_key = publicKey;
            sendRecord.recipient_public_key = "BADADDRESS___";
            sendRecord.hash = functions.getRecordHash(sendRecord);
            ecdsa.SignMessage(privateKey, sendRecord.hash, signature);
            sendRecord.signature = signature;
            
            
            // TODO: review this
            CFunctions::record_structure periodSummaryRecord;
            periodSummaryRecord.time = ts;
            periodSummaryRecord.transaction_type = CFunctions::PERIOD_SUMMARY;
            periodSummaryRecord.sender_public_key = publicKey;
            periodSummaryRecord.recipient_public_key = "___MINER_ADDRESS___"; // reward for summary inclusion goes to block creator. (Only if record does not exist.)
            periodSummaryRecord.signature = "TO DO";
            periodSummaryRecord.hash = functions.getRecordHash(periodSummaryRecord);
            ecdsa.SignMessage(privateKey, periodSummaryRecord.hash, signature);
            periodSummaryRecord.signature = signature;
            
            previous_block = functions.getLastBlock("main");
            
            CFunctions::block_structure block;
            block.creator_key = publicKey;
            block.records.push_back(blockRewardRecord);
            block.records.push_back(sendRecord);
            block.records.push_back(periodSummaryRecord);
            
            block.number = selector.getCurrentTimeBlock(); //  blockNumber++;
            block.time = ts;
            
            //std::cout << " XXX \n";
            long latestBlockId = blockDB.getLatestBlockId();
            if(latestBlockId == -1){
                std::cout << "ERROR: Block generation without previous block assigned. \n";
            }
            block.previous_block_id = latestBlockId;
            
            // Add records from queue...
            std::vector< CFunctions::record_structure > records = functions.parseQueueRecords();
            for(int i = 0; i < records.size(); i++){
                //printf(" record n");
                block.records.push_back(records[i]);
                
                // TODO: watch time so that there is enough to broadcast the block in order to have it accepted.
            }
            
            block.previous_block_hash = previous_block.hash;
            block.hash = functions.getBlockHash(block);
            ecdsa.SignMessage(privateKey, block.hash, signature);
            block.signature = signature;
            
            blockDB.AddBlock(block);
            
            //functions.addToBlockFile(block); // depricate
            // TODO: Broadcast block to network
            //log.log("Adding new block. Send to relay client. \n");
            relayClient.sendBlock(block);
            
            //previous_block = block; // temp
            
            // Wait until the block period is over
            long currTimeBlock = selector.getCurrentTimeBlock();
            while( currTimeBlock == timeBlock ){
                usleep(1000000);
                currTimeBlock = selector.getCurrentTimeBlock();
                //std::cout << ".";
            }
            //std::cout << "block done" << std::endl;
            
        } else {
            usleep(1000000);
            //std::cout << "wait" << std::endl;
        }
        
        if(!isBuildingBlocks){
            usleep(1000000);
        }
        
    }
}

void CBlockBuilder::stop() {
    if ( !isBuildingBlocks ) return;
    isBuildingBlocks = false;
    // Join thread
}
