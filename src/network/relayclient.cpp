/**
* CRelayClient
*
* Description:
*  CRelayClient implements an interface to a relay server that routs network
*  traffic when CP2P peers are not available.
*
*   http://173.255.218.54/relay.php?action=getnodes&sender_key=109
*   http://173.255.218.54/relay.php?action=getnodes&sender_key=110
*   http://173.255.218.54/relay.php?action=sendmessage&sender_key=109&receiver_key=110&message=jsondata
*   http://173.255.218.54/relay.php?action=getmessages&sender_key=xxx&receiver_key=110
*
*/

#include "relayclient.h"
#include "wallet.h"
#include <sstream>

std::string CRelayClient::myPeerAddress;
bool CRelayClient::running;
bool CRelayClient::connected;
std::string CRelayClient::remotePeerAddress;

std::vector< CRelayClient::node_status > CRelayClient::node_statuses;


CRelayClient::CRelayClient(){
    //std::cout << "CPeerClient " << "\n " << std::endl;
    running = true;
    connected = false;
    controlling = true;
    stun_port = 3478;
    stun_addr = "$(host -4 -t A stun.stunprotocol.org | awk '{ print $4 }')";
}


static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}


/**
* getNetworkPeer
*
* Description: requests a new peer node connection from a: the current network or b: a connection server.
*/
std::string CRelayClient::getNewNetworkPeer(std::string myPeerAddress){

    std::string readBuffer;
    CURLcode res;
    CURL * curl;
    curl_global_init(CURL_GLOBAL_ALL); //pretty obvious
    curl = curl_easy_init();
    if(curl) {

        std::string publicKey;
        std::string privateKey;
        CWallet wallet;
        wallet.read(privateKey, publicKey);

        std::string url_string = "http://173.255.218.54/relay.php";
        std::string post_data = "connection_string=";
        post_data.append(myPeerAddress);
        post_data.append("&public_key=");
        post_data.append(publicKey);

        curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        //std::cout << readBuffer << std::endl;

        // TODO: parse results and write to local peer node file.
        // [{"public_key":"03695EB28563D74CCAA17070660767F045DD089C459D0A79865BC2376506B2FD21","connection_string":"2ANg JQyxT3POXQeLTNfsv0y4dA 1,2013266431,2600:3c03::f03c:91ff:fedf:82c9,36826,host 2,2013266431,96.126.105.53,35238,host"}]

        std::size_t open_paren = readBuffer.find("{");
        std::size_t close_paren = readBuffer.find("}", open_paren + 1);
        std::string section = readBuffer.substr( open_paren + 1, close_paren - open_paren -1 );
        //std::cout << ":" << section << std::endl;
        std::size_t con_start = section.find("connection_string\":\"");
        std::size_t con_end = section.rfind("\"");
        std::string con_string = section.substr(con_start + 20, con_end - con_start - 20);
        //std::cout << "___" << con_string << "___" << std::endl;

        return con_string;
    }
    return "";
}


/**
* relayNetworkThread
*
* Description: Implements relay networking with other nodes in the network through a server.
*/
void CRelayClient::relayNetworkThread(int argc, char* argv[]){

    std::string publicKey;
    std::string privateKey;
    CWallet wallet;
    wallet.read(privateKey, publicKey);

    while(running){


        std::string response = getNewNetworkPeer(publicKey);

       
        //std::cout << " relay client  " << response << std::endl;

        if(running){
            usleep(1000000 * 10); // 1 second

        }
    }
    std::cout << "Relay Network Shutdown." << std::endl;
}

void CRelayClient::exit(){
    running = false;
    //std::cout << " P2P exit " << "\n " << std::endl;
}



