// Copyright (c) 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "heartbeat.h"
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "wallet.h"
#include "network/relayclient.h"

volatile bool isRunning = true;

/**
 * heartbeatThread
 *
 * Description: Thread sends a heartbeat transaction request to the network
 *  every day so that all nodes can determine if this user is available to become
 *  the block creator when designated. If users are not active or up to date on the chain
 *  they cannot be relied on to create blocks. Constant block creation is important for
 *  network uptime.
 */
void CHeartbeat::heartbeatThread(int argc, char* argv[]){
    CECDSACrypto ecdsa;
    CFunctions functions;
    CRelayClient relayClient;
    std::string privateKey;
    std::string publicKey;
    while(isRunning){
        CWallet wallet;
        bool e = wallet.fileExists("wallet.dat");
        if(e != 0){
            // Load wallet
            wallet.read(privateKey, publicKey);
        }
        
        // Start delay
        for(int i = 0; i < (60 * 60) && isRunning; i++){ // // * 60
            usleep(1000000);
        }
        
        if(isRunning){
            //std::cout << "HEARTBEAT" << std::endl;
            // Broadcast heartbeat transaction.
            CFunctions::record_structure heartbeatRecord;
            // Time?
            heartbeatRecord.transaction_type = CFunctions::HEART_BEAT;
            
            heartbeatRecord.sender_public_key = publicKey;
            
            // Add current block id number to message.
            
            std::string message_siganture = "";
            heartbeatRecord.hash = functions.getRecordHash(heartbeatRecord);
            relayClient.sendRecord(heartbeatRecord);
        }
        
        // frequency delay, 24 hours - start delay.
        for(int i = 0; i < (60 * 60 * 23) && isRunning; i++){
            usleep(1000000);
        }
    }
}

void CHeartbeat::stop() {
    if ( !isRunning ) return;
    isRunning = false;
    // Join thread
}

