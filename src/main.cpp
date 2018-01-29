
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <thread>
#include "utilstrencodings.h"

#include "network/relayclient.h"
#include "network/p2p.h"
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

#include "functions/functions.h"

//#include "network/server.h"
#include "cli.h"
#include <unistd.h>		// sleep
#include <ctime>

//static const uint64_t BUFFER_SIZE = 1000*1000; // Temp
using namespace std;

volatile bool buildingBlocks = true;

/**
* blockBuilderThread
*
* Description:
* std::thread blockThread (blockBuilderThread);
*/
void blockBuilderThread(int argc, char* argv[]){
	CECDSACrypto ecdsa;
	CFunctions functions;
	// Check block chain for latest block information.
	// TODO...

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
        functions.parseBlockFile( publicKey );

	bool networkGenesis = false;
        std::string networkName = "main";
        if(argc >= 3){
            	networkGenesis = true;
		networkName = argv[2];
		//std::cout << " 1 " << argv[1] << " " << argv[2] ;
        }

	if(networkGenesis){
		std::cout << "Creating genesis block for a new network named " << networkName << std::endl;	

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
                joinRecord.sender_public_key = "";
                joinRecord.recipient_public_key = publicKey;
                std::string message_siganture = "";
                ecdsa.SignMessage(privateKey, "" + publicKey, message_siganture);
                joinRecord.message_signature = message_siganture;

                CFunctions::record_structure blockRewardRecord;
                blockRewardRecord.network = networkName;
		blockRewardRecord.time = ts;
                CFunctions::transaction_types transaction_type = CFunctions::ISSUE_CURRENCY;
                blockRewardRecord.transaction_type = transaction_type;
                blockRewardRecord.amount = 1.0;
                blockRewardRecord.recipient_public_key = publicKey;
                std::string reward_message_siganture = "";
                ecdsa.SignMessage(privateKey, "" + publicKey, reward_message_siganture);
                joinRecord.message_signature = reward_message_siganture;

		CFunctions::block_structure block;
		block.network = networkName;
                block.records.push_back(joinRecord);
                block.records.push_back(blockRewardRecord);

		time_t t = time(0);
                std::string block_time = std::asctime(std::localtime(&t));
		block.number = 0;
                block.time = block_time;

                // Calculate block hash
                std::string hash = "xxx";
                // use functions.latest_block.block_hash
                char * c_hash = (char *)malloc( 65 );
                //char c_rand[65];
                //c_rand[64] = 0;
                char *cstr = new char[hash.length() + 1];
                strcpy(cstr, hash.c_str());
                ecdsa.sha256(cstr, c_hash);
                hash = std::string(c_hash);
                delete [] cstr;
                free(c_hash);
                block.block_hash = hash;

                functions.addToBlockFile( block );
	}

	// Does a blockfile exist? if networkGenesis==false and there is no block file we wait until the network syncs before wrting to the blockfile. 

	int blockNumber = functions.latest_block.number + 1;
	while(buildingBlocks){
		

		// is it current users turn to generate a block.
		bool build_block = true;

		std::cout << " Number of users on network " <<  std::endl;


		// Scan most recent block file to set up to date user list in order to calculate which user is the block creator.

		time_t t = time(0);
		std::string block_time = std::asctime(std::localtime(&t));
  
		if(build_block){

			// While time remaining in block
			//int j = 0; 
			for(int j = 0; j < 15 && buildingBlocks; j++ ){
				usleep(1000000);
			}
			if(!buildingBlocks){
				return;
			}
		
			// If genesis block add record (  join current user, currency creation for curr user  )
			//if(argc > 1 && !strcmp(argv[1],"genesis") && !genesisCreated){
				//std::cout << " GENESIS " << std::endl;


				//genesisCreated = true;
			//}

			time_t  timev;
                        time(&timev);
                        std::stringstream ss;
                        ss << timev;
                        std::string ts = ss.str();

			/*	
			CFunctions::record_structure joinRecord;
			joinRecord.time = ts;
			CFunctions::transaction_types joinType = CFunctions::JOIN_NETWORK;
			joinRecord.transaction_type = joinType;
			joinRecord.amount = 0.0;
			joinRecord.sender_public_key = "";
			joinRecord.recipient_public_key = publicKey;
			std::string message_siganture = "";
			ecdsa.SignMessage(privateKey, "" + publicKey, message_siganture);
			joinRecord.message_signature = message_siganture;
			

			CFunctions::record_structure blockRewardRecord;
			blockRewardRecord.time = ts;
			CFunctions::transaction_types transaction_type = CFunctions::ISSUE_CURRENCY;
			blockRewardRecord.transaction_type = transaction_type;
			blockRewardRecord.amount = 1.0;
			blockRewardRecord.recipient_public_key = publicKey;
			std::string reward_message_siganture = "";
			ecdsa.SignMessage(privateKey, "" + publicKey, reward_message_siganture);
			joinRecord.message_signature = reward_message_siganture;
			*/ 

			CFunctions::record_structure sendRecord;
                        sendRecord.time = ts;
                        sendRecord.transaction_type = CFunctions::TRANSFER_CURRENCY;
                        sendRecord.amount = 0.0123;
			sendRecord.sender_public_key = publicKey;
                        sendRecord.recipient_public_key = "___BADADDRESS___";
                        std::string send_message_siganture = "";
                        ecdsa.SignMessage(privateKey, "" + publicKey, send_message_siganture);
                        sendRecord.message_signature = send_message_siganture;


			CFunctions::record_structure periodSummaryRecord;
			periodSummaryRecord.time = ts;
			periodSummaryRecord.transaction_type = CFunctions::PERIOD_SUMMARY;
			periodSummaryRecord.recipient_public_key = "___MINER_ADDRESS___"; // reward for summary inclusion goes to block creator. (Only if record does not exist.)
			periodSummaryRecord.message_signature = "TO DO";	
			

			CFunctions::block_structure block;
			//block.records.push_back(joinRecord);
			//block.records.push_back(blockRewardRecord);
			block.records.push_back(sendRecord);
			block.records.push_back(periodSummaryRecord);
            
			block.number = blockNumber++;
			block.time = block_time;
            
            
			// Add records from queue...
			//
			std::vector< CFunctions::record_structure > records = functions.parseQueueRecords();
			for(int i = 0; i < records.size(); i++){
				//printf(" record n");
                
			}

			// Calculate block hash
			std::string hash = "xxx";
			// use functions.latest_block.block_hash 
			char * c_hash = (char *)malloc( 65 );
			//char c_rand[65];
			//c_rand[64] = 0;
			char *cstr = new char[hash.length() + 1];
			strcpy(cstr, hash.c_str());

			ecdsa.sha256(cstr, c_hash);
			hash = std::string(c_hash);
			delete [] cstr;
			free(c_hash);

			block.block_hash = hash;

			functions.addToBlockFile( block );
			
		}	

		if(!buildingBlocks){
			usleep(1000000);
		}
       
	}	
}

void stop() {
    if ( !buildingBlocks ) return;
    buildingBlocks = false;
    // Join thread
}

int main(int argc, char* argv[])
{
    std::cout << ANSI_COLOR_RED << "Safire Digital Currency v0.0.1" << ANSI_COLOR_RESET << std::endl;
    std::cout << std::endl;
    // Start New BlockChain Mode
    // Read command line arg

    CFunctions functions;    
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

    functions.parseBlockFile( publicKey );
    std::cout << " Your balance: " << functions.balance << std::endl; 

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
    std::cout << "Starting networking.     [ok] " << std::endl;
    CP2P p2p;
    //p2p.getNewNetworkPeer("123"); //TEMP
    CRelayClient relayClient;

    // Validate chain
    std::cout << "Validating chain.        [ok] " << std::endl;

    // Interface type [CLI | GUI]
    // TODO: detect based on platform if GUI is supported.
    std::cout << "Interface type.          [cli] " << std::endl; 

    #ifdef __APPLE__
    std::cout << "Platform.                [OSX] " << std::endl;
    #endif
    #ifdef __linux__
    std::cout << "Platform.                [Linux] " << std::endl;
    #endif
    #ifdef _WIN32
    std::cout << "Platform.                [Windows] " << std::endl;
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

    std::thread blockThread(blockBuilderThread, argc, argv);    
    //std::thread p2pNetworkThread(&CP2P::p2pNetworkThread, p2p, argc, argv); // TODO: implement a main class to pass into threads instead of 'p2p' instance. For communication.
    std::thread relayNetworkThread(&CRelayClient::relayNetworkThread, relayClient, argc, argv); 

    CCLI cli;
    cli.processUserInput();    

    std::cout << "Shutting down... " << std::endl;
    stop();
    blockThread.join();
    p2p.exit();
    relayClient.exit();
    //p2pNetworkThread.join();
    relayNetworkThread.join();
 
    usleep(100000);
    std::cout << "Done " << std::endl;
    
}
