// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cli.h"

#include <sstream>
#include <unistd.h>   // open and close
#include <sys/stat.h> // temp because we removed util
#include <fcntl.h> // temp removed util.h
#include <time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/**
* printCommands 
*
* Description: Print a list of commands that are accepted.
*/

void CCLI::printCommands(){
	std::cout << "Commands: \n" << 
	"join - request membership in the network.\n " <<
	"balance - print balance and transaction summary.\n" <<
	"sent - print sent transaction list details.\n " <<
	"received - print received transaction list details.\n" <<
	"network - print network stats including currency and volumes.\n" <<
	"send - send a payment to another user address.\n " <<
	"receive - prints your public key address to have others send you payments.\n" <<
	"quit - shutdown the application.\n " << std::endl;
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
		} else if ( command.find("balance") != std::string::npos ){
			std::cout << "Balance: 0.0 sfr \n" << std::endl;


		} else if ( command.find("quit") != std::string::npos ){
			running = false;	
		} else {
			printCommands();	
		}

	}
}




