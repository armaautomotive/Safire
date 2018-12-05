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
#include <boost/lexical_cast.hpp>
#include "functions/chain.h"
#include "blockdb.h"
#include "log.h"

std::string CRelayClient::myPeerAddress;
bool CRelayClient::running;
bool CRelayClient::connected;
std::string CRelayClient::remotePeerAddress;

std::vector< CRelayClient::node_status > CRelayClient::node_statuses;

/**
 * CRelayClient
 * Description:
 */
CRelayClient::CRelayClient(){
    running = true;
    connected = false;
    controlling = true;
    stun_port = 3478;
    stun_addr = "$(host -4 -t A stun.stunprotocol.org | awk '{ print $4 }')";
}

/**
 * WriteCallback
 *
 * Description: Callback function.
 */
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

/**
 * getNetworkPeer
 *
 * Description: requests a new peer node connection from a: the current network or b: a connection server.
 *
 * @param: string myPeerAddress.
 * @return: string - network address.
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

/**
 * getPeers
 *
 * Description:
 */
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
        receiveRequestBlocks();

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

            // read server response
            std::stringstream ss;
            ss << readBuffer;
            std::string line;
            while(std::getline(ss,line,'\n')){
                std::cout << "Response: " << line << std::endl;
            
            }
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
    CFileLogger log;
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

            //std::cout << " SEND BLOCK " << post_data << std::endl;
            log.log("\n");
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
    CFileLogger log;
    CFunctions functions;
    CBlockDB blockDB;
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
        log.log("GETBLOCK " + readBuffer + " \n");

        std::vector< CFunctions::block_structure > blocks = functions.parseBlockJson(readBuffer);   
        for(int i = 0; i < blocks.size(); i++){
            CFunctions::block_structure block = blocks.at(i);
            //functions.addToBlockFile(block);
            
            blockDB.AddBlock(block);
            
            result = true;
        } 
    }   
    return result; 
}


/**
* sendRequestBlocks() 
*
* Description: Send a request to connected nodes (through server) asking for blocks starting at a number. 
* -1 indicates starting from the genesis block.
*
*/
void CRelayClient::sendRequestBlocks(long blockNumber){
    CFileLogger log;
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
            // http://173.255.218.54/relay.php?action=sendmessage&type=request&sender_key=109&receiver_key=110&message=-1
            
            std::string url_string = "http://173.255.218.54/relay.php";
            std::string post_data = "action=sendmessage&type=request&sender_key=";
            post_data.append(publicKey);
            post_data.append("&receiver_key=");
            post_data.append(node.public_key);
            post_data.append("&message=");
            post_data.append(boost::lexical_cast<std::string>(blockNumber));
            curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            //std::cout << " SEND request to receive block data. " << blockNumber << " to: " << node.public_key << std::endl;
            std::ostringstream logStream;
            logStream << "SEND request to receive block data. " << blockNumber + " to: " << node.public_key << "\n";
            log.log(logStream.str());
        }
    }
}

/**
* receiveRequestBlocks
*
* Description: Read requests from other nodes to send a block. 
*    Given requesting key and block number (-1 for first block) 
* 
*/
bool CRelayClient::receiveRequestBlocks(){
    bool result = false;
    CFunctions functions;
    CChain chain;
    CBlockDB blockDB;
    CFileLogger log;

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
        // http://173.255.218.54/relay.php?action=getmessages&type=request&sender_key=xxx&receiver_key=110

        std::string url_string = "http://173.255.218.54/relay.php";
        std::string post_data = "action=getmessages&type=request&sender_key=&receiver_key=";
        post_data.append(publicKey);
        post_data.append("&return_request_data=true"); // we need the sender key in order to reply to this request
        curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        //std::cout << " GETBLOCKREQUEST " << readBuffer << std::endl;
        // parse message=

        std::string blockRequestString = functions.parseSectionString(readBuffer, "\"message\":\"", "\"");
        // TODO: we need to know who is requesting the block.
        std::string sender_key = functions.parseSectionString(readBuffer, "\"sender_key\":\"", "\"");    
 
        if(blockRequestString.length() > 0){
            //std::cout << " -  Receive request for block: " << blockRequestString << " sender_key: " << sender_key << std::endl;
            log.log(" -  Receive request for block: " + blockRequestString + " sender_key: " + sender_key + "\n");
            
            std::string::size_type sz;
            long requestedBlock = std::stol(blockRequestString, &sz);
            if( requestedBlock == -1 ){ // Request genesis block onward
                
                requestedBlock = blockDB.getFirstBlockId();
                
                /*
                // if chain.firstBlock exists and > 0
                if(chain.getFirstBlock() > -1){ // de
                            requestedBlock = chain.getFirstBlock(); // requires the complete chain to be parsed to access using this approach.

                            // Read from blockDB
                            //functions:: = blockDB.getFirstBlock();
                }
                 */
            }
                     
            //std::cout << "block number to send " << requestedBlock << std::endl;
            std::ostringstream logStream;
            logStream << " block number to send " << requestedBlock << "\n";
            log.log(logStream.str());
 
            if(requestedBlock > -1){
                    CFunctions::block_structure block = blockDB.getBlock(requestedBlock);                     
                    //std::cout << "sending " << functions.blockJSON(block) << std::endl;

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
                        post_data.append(sender_key);
                        post_data.append("&message=");
                        post_data.append(functions.blockJSON(block));
                        curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
                        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
                        res = curl_easy_perform(curl);
                        curl_easy_cleanup(curl);
                        //std::cout << " SENT BLOCK ON REQUEST " << post_data << std::endl;
                        
                        log.log(" SENT BLOCK ON REQUEST " + post_data + "\n");
                    }
            }
 
        }
 
        // compose 
 
        //std::vector< CFunctions::block_structure > blocks = functions.parseBlockJson(readBuffer);
        //for(int i = 0; i < blocks.size(); i++){ 
        //    CFunctions::block_structure block = blocks.at(i);
        //    functions.addToBlockFile(block);
        //    result = true;
        //}
    }   
    return result;
}


