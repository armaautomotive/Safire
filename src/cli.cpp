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
    " tests    - Run tests to verify this build is functioning correctly.\n " <<
    std::endl;
}


/**
* processUserInput
*
* Description: process user commands and print results
*/
void CCLI::processUserInput(){
	bool running = true;
	printCommands();
	while(running){
		std::cout << ">";		

		std::string command;
		std::cin >> command;

		if( command.find("join") != std::string::npos ){
			std::cout << "Joining... \n" << std::endl;
            // Print if pending or allready accepted.
		} else if ( command.find("balance") != std::string::npos ){
			

		CFunctions functions;

            
            std::cout << "Balance: 0.0 sfr \n" << std::endl;
        } else if ( command.find("sent") != std::string::npos ){
            std::cout << "This feature is not implemented yet.\n" << std::endl;
            
        } else if ( command.find("receive") != std::string::npos ){
            
            std::cout << "This feature is not implemented yet.\n" << std::endl;
            
        } else if ( command.find("send") != std::string::npos ){
            std::cout << "Enter destination address: \n" << std::endl;
            std::string destination_address;
            std::cin >> destination_address;
            std::cout << "Enter amount to send: \n" << std::endl;
            std::string amount;
            std::cin >> amount;
            std::cout << "Sending " << amount << " to user: " << destination_address << " \n" << std::endl;

        } else if ( command.find("network") != std::string::npos ){
            
            std::cout << "This feature is not implemented yet.\n" << std::endl;
            
		} else if ( command.find("quit") != std::string::npos ){
			running = false;
            
        } else if ( command.find("advanced") != std::string::npos ){
            printAdvancedCommands();
        } else if ( command.find("tests") != std::string::npos ){
            CECDSACrypto ecdsa;
            ecdsa.runTests();
		} else {
			printCommands();	
		}

	}
}




