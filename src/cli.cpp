// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cli.h"

#include <sstream>
#include <unistd.h>   // open and close
#include <sys/stat.h> // temp because we removed util
#include <fcntl.h> // temp removed util.h
#include <time.h>
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "wallet.h"
#include "network/p2p.h"

/**
* printCommands 
*
* Description: Print a list of commands that are accepted.
*/
void CCLI::printCommands(){
	std::cout << "Commands:\n" <<
	" join     - request membership in the network.\n" <<
	" balance  - print balance and transaction summary.\n" <<
	" sent     - print sent transaction list details.\n" <<
	" received - print received transaction list details.\n" <<
	" network  - print network stats including currency and volumes.\n" <<
	" send     - send a payment to another user address.\n" <<
	" receive  - prints your public key address to have others send you payments.\n" <<
	" advanced - more commands for admin and testing functions.\n" <<
	" quit     - shutdown the application.\n " << std::endl;
    
    
}

void CCLI::printAdvancedCommands(){
    std::cout << " Advanced: \n" <<
    " tests     - Run tests to verify this build is functioning correctly.\n " <<
    " chain     - Scan the complete blockchain for verification. Reports findings.\n " <<
    std::endl;
}


/**
* processUserInput
*
* Description: process user commands and print results
*/
void CCLI::processUserInput(){
	bool running = true;
	CECDSACrypto ecdsa;
	CFunctions functions;
	std::string privateKey;
	std::string publicKey;
	CWallet wallet;
	bool e = wallet.fileExists("wallet.dat");
	if(e != 0){
		// Load wallet
		wallet.read(privateKey, publicKey);
		functions.parseBlockFile(publicKey);	
	}

	printCommands();
	while(running){
		std::cout << ">";		

		std::string command;
		std::cin >> command;

		if( command.find("join") != std::string::npos ){
			
			//CFunctions functions;
                        //std::string privateKey;
                        //std::string publicKey;
                        //CWallet wallet;
                        //bool e = wallet.fileExists("wallet.dat");
                        //if(e != 0){
                                // Load wallet
                                //wallet.read(privateKey, publicKey);
                                functions.parseBlockFile( publicKey ); 
                        //}

			if(functions.joined == true){
				std::cout << "Allready joined network. \n" << std::endl;
			} else {
				std::cout << "Joining request sending... \n" << std::endl;

				CFunctions::record_structure joinRecord;
				time_t  timev;
				time(&timev);
				std::stringstream ss;
				ss << timev;
				std::string ts = ss.str();
				joinRecord.time = ts;
				joinRecord.transaction_type = CFunctions::JOIN_NETWORK;
				joinRecord.amount = 0.0;
				joinRecord.sender_public_key = "";
				joinRecord.recipient_public_key = publicKey;
				std::string message_siganture = "";
				ecdsa.SignMessage(privateKey, "" + publicKey, message_siganture);
				joinRecord.message_signature = message_siganture;	
				functions.addToQueue( joinRecord );
	
				// TODO: send request or say allready sent. 	
			}

			// Print if pending or allready accepted.
		} else if ( command.find("balance") != std::string::npos ){
			

			//CFunctions functions;
			//std::string privateKey;
			//std::string publicKey;
			//CWallet wallet;
			//bool e = wallet.fileExists("wallet.dat");
			//if(e != 0){
				// Load wallet
				//wallet.read(privateKey, publicKey);
    
				functions.parseBlockFile( publicKey );
				std::cout << " Your balance: " << functions.balance << " sfr" << std::endl;

			//}
		} else if ( command.find("sent") != std::string::npos ){
			std::cout << "This feature is not implemented yet.\n" << std::endl;
            
        } else if ( command.find("receive") != std::string::npos ){
            
            std::cout << "Your receiving address is: " << publicKey << "\n" << std::endl;
 
        } else if ( command.find("send") != std::string::npos ){
            std::cout << "Enter destination address: \n" << std::endl;
            std::string destination_address;
            std::cin >> destination_address;
            std::cout << "Enter amount to send: \n" << std::endl;
            std::string amount;
            std::cin >> amount;
            std::cout << "Sending " << amount << " to user: " << destination_address << " \n" << std::endl;

		double d_amount = ::atof(amount.c_str());

		// TODO also check balance adjusted using sent requests in queue....
		// TODO check destination address is an accepted user
		if(d_amount > functions.balance ){
			std::cout << "Insuficient balance. Unable to send transfer request. " << std::endl;
		} else {

			CFunctions::record_structure sendRecord;
			time_t  timev;
			time(&timev);
			std::stringstream ss;
			ss << timev;
			std::string ts = ss.str();
			sendRecord.time = ts;
			sendRecord.transaction_type = CFunctions::TRANSFER_CURRENCY;
			sendRecord.amount = d_amount;
			sendRecord.sender_public_key = publicKey;
			sendRecord.recipient_public_key = publicKey;
			std::string message_siganture = destination_address;
			ecdsa.SignMessage(privateKey, "" + publicKey, message_siganture);
			sendRecord.message_signature = message_siganture;
			functions.addToQueue( sendRecord );	
			std::cout << "Sent transfer request. " << std::endl;	
		}

        } else if ( command.find("network") != std::string::npos ){

		functions.parseBlockFile( publicKey );
                std::cout << " Joined network: " << (functions.joined > 0 ? "yes" : "no") << std::endl;
		std::cout << " Currency supply: " << functions.currency_circulation << " sfr" << std::endl;

		// Block chain size?
	 	std::cout << " Blockchain size: " << " 0MB" << std::endl;
	
		std::cout << " Pending transactions: " << " 0" << std::endl;  
			
		// Active connections?
		//std::cout << "This feature is not implemented yet.\n" << std::endl;
           
		CP2P p2p;
		std::cout << " Peer Address: " << p2p.myPeerAddress << std::endl;
                
                p2p.sendData("DATA DATA DATA 123 \0");

 
	} else if ( command.find("quit") != std::string::npos ){
			running = false;
            
        } else if ( command.find("advanced") != std::string::npos ){
            printAdvancedCommands();
        } else if ( command.find("tests") != std::string::npos ){
            CECDSACrypto ecdsa;
            ecdsa.runTests();

	} else if ( command.find("chain") != std::string::npos ){
           std::cout << " Blockchain state: " << " Not implemented. " << std::endl;
		
	} else {
		printCommands();	
	}
    }
}




