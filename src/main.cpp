// Copyright (c) 2016 2017 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <thread>
#include "utilstrencodings.h"
#include "global.h"
#include "network/relayclient.h"
#include "network/p2p.h"
#include "functions/selector.h"
#include "functions/chain.h"
//#include "wallet/wallet.h"
//#include "key.h"
//#include "pubkey.h"

//#include <openssl/crypto.h> // no worky
//#include <openssl/ec.h>
#include <stdio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

//#include "rsacrypto.h"
#include "ecdsacrypto.h"
#include "wallet.h"
#include "transaction.h"

//#include "userdb.h"
//#include "leveldb/db.h" // TEMP 
#include "blockdb.h"
#include "log.h"

#include "functions/functions.h"
#include "functions/blockbuilder.hpp" // Revierw this later.

//#include "network/server.h"
#include "cli.h"
#include <unistd.h>		// sleep
#include <ctime>

//static const uint64_t BUFFER_SIZE = 1000*1000; // Temp
using namespace std;

volatile bool buildingBlocks = true;

int main(int argc, char* argv[])
{
    std::cout << ANSI_COLOR_RED << "Safire Digital Currency v0.0.1.02" << ANSI_COLOR_RESET << std::endl;
    std::cout << std::endl;
    
    std::cout << "a \n";
    
    CBlockDB blockDB;
    
    std::cout << " getDB \n";
    
    leveldb::DB * db = blockDB.getDatabase();
    
    std::cout << "b \n";
    
    CFileLogger log;
    log.clearLog();
    log.log("Safire starting.\n");
    
    // Check for node reset command
    if(argc >= 2 && strcmp(argv[1], "reset") == 0){
       std::cout << "Reseting Node... \n";
       blockDB.DeleteAll();
       return 1;
    }
    
    // Start New BlockChain Mode
    // Read command line arg

    CFunctions functions;
    CBlockBuilder blockBuilder;
    /*
    CFunctions::record_structure record;
    record.time = "2017/06/03";
    CFunctions::transaction_types type = CFunctions::JOIN_NETWORK;
    record.transaction_type = type;
    record.amount = 0.1;
    record.sender_public_key = "Fsadhjsd6576asdaDGHSjghjasdASD";
    record.recipient_public_key = "SdhahBDGDhahsdbb6Bsdjj2dhjk";
    record.message_signature = "HDAJSHABbhdhsjagdbJHSDGJha";
    
    CFunctions functions;
    //functions.addToQueue(record);
    std::vector< CFunctions::record_structure > records = functions.parseQueueRecords();
    for(int i = 0; i < records.size(); i++){
        printf(" record n");
    }
    */
    CChain chain; 
    
    //std::string p;
    //std::string v;
    //std::string u;
    //std::cout << "  " << std::endl;	
    CECDSACrypto ecdsa;
    //int r = ecdsa.GetKeyPair(p, v, u );
    //std::cout << "  private  " << p << "\n  public " << v << "\n  Public compressed: " << u << std::endl; 
    //ecdsa.runTests();

    std::string privateKey;
    std::string publicKey;

    CWallet wallet;
    bool e = wallet.fileExists("wallet.dat");
    //printf(" wallet exists: %d  \n", e);
    if(e == 0){
        printf("No wallet found. Creating a new one...\n");
        std::string publicKeyUncompressed;
        int r = ecdsa.RandomPrivateKey(privateKey);
        r = ecdsa.GetPublicKey(privateKey, publicKeyUncompressed, publicKey);        
        wallet.write(privateKey, publicKey);
    } else {
        // Load wallet
        wallet.read(privateKey, publicKey);
    }
    std::cout <<
        //"  private  " << privateKey << "\n  " <<
        " Your public address: " << publicKey << std::endl;

    //functions.parseBlockFile( publicKey, false );
    functions.scanChain(publicKey, false);
    std::cout << " Your balance: " << functions.balance << " sfr" << std::endl; 

    std::cout << " Joined network: " << (functions.joined > 0 ? "yes" : "no") << std::endl;

    std::cout << std::endl;

    // Transactions
    /*
    CTransaction transaction;
    std::string join = transaction.joinNetwork(publicKey);
    std::cout << " join: " << join << std::endl;	
    std::string sendPayment = transaction.sendPayment( privateKey, publicKey, u, 23.45, 1);
    std::cout << " payment: " << sendPayment << std::endl;
     */
    
    //CUserDB userDB;
    //userDB.AddUser("test", "127.0.0.1");
    //userDB.GetUsers();


    //CBlockDB blockDB;
    //blockDB.AddBlock("First");
    //blockDB.GetBlocks();


    // Start Networking
    std::cout << " Starting networking.    " << ANSI_COLOR_GREEN << "[ok] " << ANSI_COLOR_RESET << std::endl;
    //CP2P p2p;
    //p2p.getNewNetworkPeer("123"); //TEMP
    CRelayClient relayClient;
    relayClient.getNewNetworkPeer(publicKey);

    // Validate chain
    std::cout << " Validating chain.       " << ANSI_COLOR_GREEN << "[ok] " << ANSI_COLOR_RESET << std::endl;

    // Interface type [CLI | GUI]
    // TODO: detect based on platform if GUI is supported.
    std::cout << " Interface type.         [cli] " << std::endl; 

    #ifdef __APPLE__
    std::cout << " Platform.               [OSX] " << std::endl;
    #endif
    #ifdef __linux__
    std::cout << " Platform.               [Linux] " << std::endl;
    #endif
    #ifdef _WIN32
    std::cout << " Platform.               [Windows] " << std::endl;
    #endif

    //std::size_t num_threads = 10;
    //http::server3::server s("0.0.0.0", "80", "/Users/jondtaylor/Dropbox/Currency", num_threads);

    // Run the server until stopped.
    //s.run();
    // std::thread webserver_thread (foo); // void foo()  
    
    // If not allready, send network request to join.
/*
    CFunctions::record_structure joinRecord;
    time_t  timev;
    time(&timev);
    std::stringstream ss;
    ss << timev;
    std::string ts = ss.str();
    joinRecord.time = ts;
    CFunctions::transaction_types joinType = CFunctions::JOIN_NETWORK;
    joinRecord.transaction_type = type;
    joinRecord.amount = 0.0;
    joinRecord.sender_public_key = publicKey;
    joinRecord.recipient_public_key = "";
    std::string message_siganture = "";
    ecdsa.SignMessage(privateKey, "add_user" + publicKey, message_siganture);
    joinRecord.message_signature = message_siganture;
    //functions.addToQueue(joinRecord);

    CFunctions::block_structure block;
    block.records.push_back(joinRecord);
    block.number = 0;
    
    //functions.addToBlockFile( block );
*/
    //functions.parseBlockFile();

    // blockBuilder
    std::thread blockThread(&CBlockBuilder::blockBuilderThread, blockBuilder, argc, argv);
    
    //std::thread p2pNetworkThread(&CP2P::p2pNetworkThread, p2p, argc, argv); // TODO: implement a main class to pass into threads instead of 'p2p' instance. For communication.
    std::thread relayNetworkThread(&CRelayClient::relayNetworkThread, relayClient, argc, argv); 

    CCLI cli;
    std::cout << std::endl; // Line break
    cli.processUserInput();    

    std::cout << "Shutting down... " << std::endl;
    chain.writeFile();
    blockBuilder.stop();
    blockThread.join();
    //p2p.exit();
    relayClient.exit();
    //p2pNetworkThread.join();
    relayNetworkThread.join();
 
    usleep(100000);
    std::cout << "Done " << std::endl;
    
}
