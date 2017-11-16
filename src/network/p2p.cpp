/**
* CP2P
*
* Description: 
*/

#include "p2p.h"



CP2P::CP2P(){
    std::cout << "P2P " << "\n " << std::endl;    
 
    controlling = true;
    stun_port = 3478;
    stun_addr = "$(host -4 -t A stun.stunprotocol.org | awk '{ print $4 }')";

    g_networking_init(); 


}
