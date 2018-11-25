#include "log.h"

#include <sstream>
#include <unistd.h>   // open and close
#include <sys/stat.h> // temp because we removed util
#include <fcntl.h> // temp removed util.h
#include <time.h>
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "wallet.h"
#include "network/p2p.h"
#include "network/relayclient.h"
#include "blockdb.h"

/**
* printCommands
*
* Description: Print a list of commands that are accepted.
*/
void CLOG::log(std::string str){

        std::cout << "Commands:\n" <<
        " join [network name]     - request membership in the network. The default network is 'main'.\n" <<
        " switch [network name]   - switch current network. Default is 'main'.\n" <<
        //" swtch wallet            - change wallet" <<
        " balance                 - print balance and transaction summary.\n" <<
        " sent                    - print sent transaction list details.\n" <<
        " received                - print received transaction list details.\n" <<
        " network                 - print network stats including currency and volumes.\n" <<
        " send                    - send a payment to another user address.\n" <<
        " receive                 - prints your public key address to have others send you payments.\n" <<
        " vote                    - vote on network behaviour and settings.\n" <<
        " advanced                - more commands for admin and testing functions.\n" <<
        " quit                    - shutdown the application.\n " << std::endl;
}


