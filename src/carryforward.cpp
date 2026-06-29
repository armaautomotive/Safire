// Copyright (c) 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "carryforward.h"
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "wallet.h"
#include "network/relayclient.h"

volatile bool isRunningX = true;

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
        
        // Carry-forward records now require a checkpoint block, period id, and
        // balance snapshot. Use the CLI carryforward command until automatic
        // generation can share that validation path.
        
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
