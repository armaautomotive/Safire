/**
* CRelayClient
*
* Description:
*  CRelayClient implements an interface to a relay server that routs network
*  traffic when CP2P peers are not available.
*
*   http://173.255.218.54/relay.php?action=getnodes&sender_key=109
*   http://173.255.218.54/relay.php?action=getnodes&sender_key=110
*   http://173.255.218.54/relay.php?action=sendmessage&type=trans&sender_key=109&receiver_key=110&message=jsondata
*   http://173.255.218.54/relay.php?action=getmessages&type=block&sender_key=xxx&receiver_key=110
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
    curl_global_init(CURL_GLOBAL_ALL); 
    curl = curl_easy_init();
    if(curl) {
        std::string publicKey;
        std::string privateKey;
        CWallet wallet;
        wallet.read(privateKey, publicKey);

	// http://173.255.218.54/relay.php?action=getnodes&sender_key=109
        std::string url_string = "http://173.255.218.54/relay.php";
        std::string post_data = "action=getnodes&sender_key=";
        post_data.append(publicKey);

        curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        // TODO: parse results and write to local peer node file.
        std::string openTag = "\"public_key\":\"";
        std::size_t open = readBuffer.find(openTag);
	if(open != std::string::npos){
        	std::size_t close = readBuffer.find("\"", open + openTag.length());
        	while(open != std::string::npos && close != std::string::npos){
			std::string key_section = readBuffer.substr( open + openTag.length(), close - open - openTag.length());
        		//std::cout << "_" << key_section << "_ " << std::endl;
			bool exists = false;
			for(int i = 0; i < node_statuses.size(); i++){
				CRelayClient::node_status node = node_statuses.at(i);
				if(node.public_key.compare(key_section) == 0){
					exists = true;
				}
			}
			if(exists == false && key_section.length() > 10){
				CRelayClient::node_status node;
				node.public_key = key_section;
				node_statuses.push_back(node);
			}
			readBuffer = readBuffer.substr( close + 1 );	
			open = readBuffer.find("\"public_key\":\"");
			if(open != std::string::npos){
				close = readBuffer.find("\"", open + openTag.length());		
			}
		}
	}
    }
    return "";
}

std::vector<CRelayClient::node_status> CRelayClient::getPeers(){
	return node_statuses;
}




/**
* relayNetworkThread
*
* Description: Implements relay networking with other nodes in the network through a server.
*/
void CRelayClient::relayNetworkThread(int argc, char* argv[]){
    CFunctions functions;
    std::string publicKey;
    std::string privateKey;
    CWallet wallet;
    wallet.read(privateKey, publicKey);
    while(running){
        receiveRecords(); // receive transaction records
        if(receiveBlocks()){  // receive blocks and write to file
             functions.parseBlockFile("", false); // parse block file changes
        }
        // TODO: requests for blocks by id

        // If there are no active peers, get a new list.
        //std::string response = getNewNetworkPeer(publicKey);
        //std::cout << " relay client  " << response << std::endl;

        if(running){
            usleep(1000000 * 2); // 1 second
        }
    }
    std::cout << "Relay Network Shutdown." << std::endl;
}


/**
* exit
*
* Description: shut down thread for exit.
*/
void CRelayClient::exit(){
    running = false;
}


/**
* sendRecord - broadcastRecord
*
* Description: send a transaction record to another/all connected node
*/
void CRelayClient::sendRecord(CFunctions::record_structure record){
    CFunctions functions;
    std::string publicKey;
    std::string privateKey;
    CWallet wallet;
    wallet.read(privateKey, publicKey);
    for(int i = 0; i < node_statuses.size(); i++){
        CRelayClient::node_status node = node_statuses.at(i);
        std::string readBuffer;
        CURLcode res;
        CURL * curl;
        curl_global_init(CURL_GLOBAL_ALL); 
        curl = curl_easy_init();
        if(curl) {
            // http://173.255.218.54/relay.php?action=sendmessage&type=trans&sender_key=109&receiver_key=110&message=jsondata 
	    std::string url_string = "http://173.255.218.54/relay.php";
            std::string post_data = "action=sendmessage&type=trans&sender_key=";
            post_data.append(publicKey); // record.sender_public_key
            post_data.append("&receiver_key=");
            post_data.append(node.public_key);
            post_data.append("&message=");
            post_data.append(functions.recordJSON(record));  
            curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }
    }
}

/**
* receiveRecords
*
* Description: read records sent to this node from the server
*/
void CRelayClient::receiveRecords(){
    CFunctions functions;
    std::string readBuffer;
    CURLcode res;
    CURL * curl;
    curl_global_init(CURL_GLOBAL_ALL); 
    curl = curl_easy_init();
    if(curl) {
        std::string publicKey;
        std::string privateKey;
        CWallet wallet;
        wallet.read(privateKey, publicKey);

        // http://173.255.218.54/relay.php?action=getmessages&type=trans&sender_key=xxx&receiver_key=110
        std::string url_string = "http://173.255.218.54/relay.php";
        std::string post_data = "action=getmessages&type=trans&sender_key=&receiver_key=";
        post_data.append(publicKey);

        curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
 
        // Write to queue file
        std::stringstream ss;
        ss << readBuffer;
        std::string line;
        while(std::getline(ss,line,'\n')){
            //std::cout << "line " << line << std::endl;
            CFunctions::record_structure record;
            record = functions.parseRecordJson(line);    
            if( record.sender_public_key.length() > 0 || record.recipient_public_key.length() > 0 ){
                functions.addToQueue(record); 
                //std::cout << " *** ";
            }       
        } 
    }
}

/**
* sendBlock
*
* Description: send a block to each connected peer.
*/
void CRelayClient::sendBlock(CFunctions::block_structure block){
    CFunctions functions;
    std::string publicKey;
    std::string privateKey;
    CWallet wallet;
    wallet.read(privateKey, publicKey);
    for(int i = 0; i < node_statuses.size(); i++){
        CRelayClient::node_status node = node_statuses.at(i);
        std::string readBuffer;
        CURLcode res;
        CURL * curl;
        curl_global_init(CURL_GLOBAL_ALL); 
        curl = curl_easy_init();
        if(curl) {
            // http://173.255.218.54/relay.php?action=sendmessage&type=block&sender_key=109&receiver_key=110&message=jsondata
            std::string url_string = "http://173.255.218.54/relay.php";
            std::string post_data = "action=sendmessage&type=block&sender_key=";
            post_data.append(publicKey);
            post_data.append("&receiver_key=");
            post_data.append(node.public_key);
            post_data.append("&message=");
            post_data.append(functions.blockJSON(block));
            curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            //std::cout << " SEND BLOCK " << functions.blockJSON(block) << std::endl;
        }
    }
}

/**
* receiveBlocks
*
* Description: retrieve all blocks sent to this client.
*/
bool CRelayClient::receiveBlocks(){
    bool result = false;
    CFunctions functions;
    std::string readBuffer;
    CURLcode res;
    CURL * curl;
    curl_global_init(CURL_GLOBAL_ALL); 
    curl = curl_easy_init();
    if(curl) {
        std::string publicKey;
        std::string privateKey;
        CWallet wallet;
        wallet.read(privateKey, publicKey);
        // http://173.255.218.54/relay.php?action=getmessages&type=block&sender_key=xxx&receiver_key=110
        std::string url_string = "http://173.255.218.54/relay.php";
        std::string post_data = "action=getmessages&type=block&sender_key=&receiver_key=";
        post_data.append(publicKey);
        curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        //std::cout << " GETBLOCK " << readBuffer << std::endl;

        std::vector< CFunctions::block_structure > blocks = functions.parseBlockJson(readBuffer);   
        for(int i = 0; i < blocks.size(); i++){
            CFunctions::block_structure block = blocks.at(i);
            functions.addToBlockFile(block);
            result = true;
        } 
    }   
    return result; 
}




