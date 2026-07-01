// Copyright (c) 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "carryforward.h"
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "wallet.h"
#include "network/relayclient.h"
#include "network/localpeerclient.h"
#include "networkconfig.h"
#include "networktime.h"
#include "blockdb.h"

volatile bool isRunningX = true;

namespace {

long parseCarryForwardValueLong(const std::string& value, const std::string& key){
    std::string prefix = key + "=";
    std::size_t start = value.find(prefix);
    if(start == std::string::npos){
        return -1;
    }
    start += prefix.length();
    std::size_t end = value.find(";", start);
    std::string section = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if(section.length() == 0){
        return -1;
    }
    return ::atol(section.c_str());
}

std::string carryForwardValue(long checkpointBlock, long period){
    std::stringstream ss;
    ss << "checkpoint=" << checkpointBlock << ";period=" << period << ";reward=" << CFunctions::CARRY_FORWARD_REWARD;
    return ss.str();
}

std::string carryForwardUniqueKey(const CFunctions::record_structure& record){
    long period = parseCarryForwardValueLong(record.value, "period");
    if(record.recipient_public_key.length() == 0 || period < 0){
        return "";
    }

    std::stringstream ss;
    ss << record.recipient_public_key << ":" << period;
    return ss.str();
}

bool isBasicCarryForwardRecord(const CFunctions::record_structure& record, long blockNumber){
    if(record.transaction_type != CFunctions::CARRY_FORWARD){
        return false;
    }
    if(record.sender_public_key.length() == 0 || record.recipient_public_key.length() == 0){
        return false;
    }
    long checkpoint = parseCarryForwardValueLong(record.value, "checkpoint");
    long period = parseCarryForwardValueLong(record.value, "period");
    if(checkpoint < 0 || period < 0 || checkpoint >= blockNumber){
        return false;
    }
    if(record.amount < 0){
        return false;
    }
    return true;
}

double accountBalanceAtBlock(const std::string& accountPublicKey, long checkpointBlock){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(accountPublicKey.length() == 0 || firstBlockId < 0 || latestBlockId < 0){
        return 0.0;
    }

    double balance = 0.0;
    std::map<std::string, bool> acceptedCarryForwardKeys;
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        if(block.number > checkpointBlock){
            break;
        }
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.transaction_type == CFunctions::ISSUE_CURRENCY &&
               record.recipient_public_key.compare(accountPublicKey) == 0){
                balance += record.amount;
            } else if(record.transaction_type == CFunctions::TRANSFER_CURRENCY){
                if(record.recipient_public_key.compare(accountPublicKey) == 0){
                    balance += record.amount;
                }
                if(record.sender_public_key.compare(accountPublicKey) == 0 &&
                   record.recipient_public_key.compare(accountPublicKey) != 0){
                    balance -= record.amount;
                    balance -= record.fee;
                }
                if(block.creator_key.compare(accountPublicKey) == 0){
                    balance += record.fee;
                }
            } else if(record.transaction_type == CFunctions::VOTE){
                if(record.sender_public_key.compare(accountPublicKey) == 0){
                    balance -= record.fee;
                }
                if(block.creator_key.compare(accountPublicKey) == 0){
                    balance += record.fee;
                }
            } else if(isBasicCarryForwardRecord(record, block.number)){
                std::string key = carryForwardUniqueKey(record);
                if(key.length() > 0 && acceptedCarryForwardKeys.find(key) == acceptedCarryForwardKeys.end()){
                    acceptedCarryForwardKeys[key] = true;
                    if(record.sender_public_key.compare(accountPublicKey) == 0){
                        balance += CFunctions::CARRY_FORWARD_REWARD;
                    }
                }
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
    return balance;
}

bool carryForwardSnapshotMatches(const CFunctions::record_structure& record){
    long checkpoint = parseCarryForwardValueLong(record.value, "checkpoint");
    if(checkpoint < 0){
        return false;
    }

    double expectedBalance = accountBalanceAtBlock(record.recipient_public_key, checkpoint);
    double diff = expectedBalance - record.amount;
    if(diff < 0){
        diff = -diff;
    }
    return diff < 0.000001;
}

bool acceptedCarryForwardExists(const std::string& accountPublicKey, long period){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(accountPublicKey.length() == 0 || firstBlockId < 0 || latestBlockId < 0){
        return false;
    }

    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.transaction_type == CFunctions::CARRY_FORWARD &&
               record.recipient_public_key.compare(accountPublicKey) == 0 &&
               parseCarryForwardValueLong(record.value, "period") == period &&
               isBasicCarryForwardRecord(record, block.number) &&
               carryForwardSnapshotMatches(record)){
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

bool pendingCarryForwardExists(CFunctions& functions, const std::string& accountPublicKey, long period){
    std::vector<CFunctions::record_structure> records = functions.peekQueueRecords();
    for(int i = 0; i < records.size(); i++){
        CFunctions::record_structure record = records.at(i);
        if(record.transaction_type == CFunctions::CARRY_FORWARD &&
           record.recipient_public_key.compare(accountPublicKey) == 0 &&
           parseCarryForwardValueLong(record.value, "period") == period){
            return true;
        }
    }
    return false;
}

bool queueCarryForwardIfDue(
    CFunctions& functions,
    CRelayClient& relayClient,
    CECDSACrypto& ecdsa,
    const std::string& privateKey,
    const std::string& publicKey
){
    CBlockDB blockDB;
    CNetworkConfig storagePolicy = CNetworkConfig::load();
    long checkpointAgeBlocks = storagePolicy.carryForwardCheckpointAgeBlocks();
    long periodBlocks = storagePolicy.carryForwardPeriodBlocks();
    long firstBlockId = blockDB.getFirstBlockId();
    long latestConnectedBlockId = blockDB.getConnectedLatestBlockId();
    if(privateKey.length() == 0 || publicKey.length() == 0 || firstBlockId < 0 || latestConnectedBlockId < 0){
        return false;
    }
    if(checkpointAgeBlocks <= 0 || periodBlocks <= 0 || latestConnectedBlockId - firstBlockId < checkpointAgeBlocks){
        return false;
    }

    long checkpointBlock = latestConnectedBlockId - checkpointAgeBlocks;
    long period = checkpointBlock / periodBlocks;
    if(acceptedCarryForwardExists(publicKey, period) || pendingCarryForwardExists(functions, publicKey, period)){
        return false;
    }

    double checkpointBalance = accountBalanceAtBlock(publicKey, checkpointBlock);
    if(checkpointBalance <= 0){
        return false;
    }

    CFunctions::record_structure carryForwardRecord;
    carryForwardRecord.network = "main";
    CNetworkTime netTime;
    std::stringstream ss;
    ss << netTime.getEpoch();
    carryForwardRecord.time = ss.str();
    carryForwardRecord.transaction_type = CFunctions::CARRY_FORWARD;
    carryForwardRecord.amount = checkpointBalance;
    carryForwardRecord.fee = 0.0;
    carryForwardRecord.sender_public_key = publicKey;
    carryForwardRecord.recipient_public_key = publicKey;
    carryForwardRecord.name = "carry-forward";
    carryForwardRecord.value = carryForwardValue(checkpointBlock, period);
    carryForwardRecord.hash = functions.getRecordHash(carryForwardRecord);
    std::string signature = "";
    ecdsa.SignMessage(privateKey, carryForwardRecord.hash, signature);
    carryForwardRecord.signature = signature;

    if(functions.addToQueue(carryForwardRecord)){
        relayClient.sendRecord(carryForwardRecord);
        CLocalPeerClient::broadcastRecord(carryForwardRecord);
        return true;
    }
    return false;
}

}

/**
 * heartbeatThread
 *
 * Description: Thread summarizes past years records into a summary record and
 *     broadcatss it to the network. Eventually old transaction records can be deleted to save
 *      storage and sync resources.
 */
void CCarryforward::carryforwardThread(int argc, char* argv[]){
    CECDSACrypto ecdsa;
    CFunctions functions;
    CRelayClient relayClient;
    std::string privateKey;
    std::string publicKey;
    while(isRunningX){
        CWallet wallet;
        bool e = wallet.fileExists("wallet.dat");
        if(e != 0){
            // Load wallet
            wallet.read(privateKey, publicKey);
        }
        
        // Start delay
        for(int i = 0; i < (60 * 60) && isRunningX; i++){ // // * 60
            usleep(1000000);
        }
        
        if(isRunningX){
            queueCarryForwardIfDue(functions, relayClient, ecdsa, privateKey, publicKey);
        }
        
        // frequency delay, 24 hours - start delay.
        for(int i = 0; i < (60 * 60 * 23) && isRunningX; i++){
            usleep(1000000);
        }
    }
}

void CCarryforward::stop() {
    if ( !isRunningX ) return;
    isRunningX = false;
    // Join thread
}
