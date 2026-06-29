// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cli.h"

#include <deque>
#include <sstream>
#include <unistd.h>   // open and close
#include <sys/stat.h> // temp because we removed util
#include <fcntl.h> // temp removed util.h
#include <time.h>
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "functions/selector.h"
#include "wallet.h"
#include "network/p2p.h"
#include "network/relayclient.h"
#include "network/localpeerclient.h"
#include "blockdb.h"
#include "userdb.h"

namespace {

std::string recordTypeName(CFunctions::transaction_types type){
    if(type == CFunctions::JOIN_NETWORK){
        return "JOIN_NETWORK";
    }
    if(type == CFunctions::ISSUE_CURRENCY){
        return "ISSUE_CURRENCY";
    }
    if(type == CFunctions::TRANSFER_CURRENCY){
        return "TRANSFER_CURRENCY";
    }
    if(type == CFunctions::CARRY_FORWARD){
        return "CARRY_FORWARD";
    }
    if(type == CFunctions::PERIOD_SUMMARY){
        return "PERIOD_SUMMARY";
    }
    if(type == CFunctions::VOTE){
        return "VOTE";
    }
    if(type == CFunctions::HEART_BEAT){
        return "HEART_BEAT";
    }
    return "UNKNOWN";
}

std::string shortKey(const std::string& key){
    if(key.length() == 0){
        return "-";
    }
    if(key.length() <= 16){
        return key;
    }
    return key.substr(0, 12) + "...";
}

std::string formatSlotTime(long timeBlock){
    time_t slotTime = (time_t)(timeBlock * 15);
    struct tm * timeInfo = localtime(&slotTime);
    if(timeInfo == NULL){
        return "-";
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
    return std::string(buffer);
}

std::vector<CFunctions::record_structure> acceptedMembershipRecords(){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    std::vector<CFunctions::record_structure> members;
    if(firstBlockId < 0 || latestBlockId < 0){
        return members;
    }

    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.transaction_type == CFunctions::JOIN_NETWORK){
                bool exists = false;
                for(int m = 0; m < members.size(); m++){
                    if(members.at(m).sender_public_key.compare(record.sender_public_key) == 0){
                        exists = true;
                    }
                }
                if(exists == false){
                    members.push_back(record);
                }
            }
        }
        if(block.number == latestBlockId){
            break;
        }
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
        if(nextBlock.number <= 0 || nextBlock.number == block.number){
            break;
        }
        block = nextBlock;
        guard++;
    }
    return members;
}

void printSelectedBlockCreator(const std::vector<CFunctions::record_structure>& members, long timeBlock, const std::string& label, const std::string& localPublicKey){
    std::cout << " " << label << " block: " << timeBlock << std::endl;
    std::cout << " Time: " << formatSlotTime(timeBlock) << std::endl;
    if(members.size() == 0){
        std::cout << " Creator: none selected; no accepted membership records loaded." << std::endl;
        return;
    }

    long userIndex = timeBlock % members.size();
    CFunctions::record_structure member = members.at(userIndex);
    std::string name = member.name;
    std::cout << " Creator: " << member.sender_public_key << std::endl;
    std::cout << " Name: " << (name.length() > 0 ? name : "-") << std::endl;
    if(label.compare("Current") == 0){
        std::cout << " This node: " << (member.sender_public_key.compare(localPublicKey) == 0 ? "yes" : "no") << std::endl;
    }
}

void printNextBlockSelection(const std::string& localPublicKey){
    CSelector selector;
    long currentBlock = selector.getCurrentTimeBlock();
    long nextBlock = currentBlock + 1;
    std::vector<CFunctions::record_structure> members = acceptedMembershipRecords();
    time_t now;
    time(&now);
    long secondsUntilNextBlock = (nextBlock * 15) - now;
    if(secondsUntilNextBlock < 0){
        secondsUntilNextBlock = 0;
    }

    std::cout << " Block creator selection:" << std::endl;
    std::cout << " Accepted members: " << members.size() << std::endl;
    printSelectedBlockCreator(members, currentBlock, "Current", localPublicKey);
    std::cout << " Seconds until next block: " << secondsUntilNextBlock << std::endl;
    printSelectedBlockCreator(members, nextBlock, "Next", localPublicKey);
}

std::string recordSummary(long blockNumber, int recordIndex, CFunctions::record_structure record, bool fullKeys){
    std::stringstream ss;
    ss << " block " << blockNumber << " #" << recordIndex << " " << recordTypeName(record.transaction_type);
    ss << " time " << (record.time.length() > 0 ? record.time : "-");
    ss << " net " << (record.network.length() > 0 ? record.network : "-");
    ss << " amt " << record.amount;
    ss << " fee " << record.fee;
    if(record.transaction_type == CFunctions::JOIN_NETWORK){
        ss << " member " << (fullKeys ? record.sender_public_key : shortKey(record.sender_public_key));
    } else {
        ss << " from " << (fullKeys ? record.sender_public_key : shortKey(record.sender_public_key));
        ss << " to " << (fullKeys ? record.recipient_public_key : shortKey(record.recipient_public_key));
    }
    if(record.name.length() > 0){
        ss << " name \"" << record.name << "\"";
    }
    if(record.value.length() > 0){
        ss << " value \"" << record.value << "\"";
    }
    if(record.hash.length() > 0){
        ss << " hash " << shortKey(record.hash);
    }
    return ss.str();
}

std::string mempoolRecordSummary(int recordIndex, CFunctions::record_structure record, bool fullKeys){
    std::stringstream ss;
    ss << " mempool #" << recordIndex << " " << recordTypeName(record.transaction_type);
    ss << " time " << (record.time.length() > 0 ? record.time : "-");
    ss << " net " << (record.network.length() > 0 ? record.network : "-");
    ss << " amt " << record.amount;
    ss << " fee " << record.fee;
    if(record.transaction_type == CFunctions::JOIN_NETWORK){
        ss << " member " << (fullKeys ? record.sender_public_key : shortKey(record.sender_public_key));
    } else {
        ss << " from " << (fullKeys ? record.sender_public_key : shortKey(record.sender_public_key));
        ss << " to " << (fullKeys ? record.recipient_public_key : shortKey(record.recipient_public_key));
    }
    if(record.name.length() > 0){
        ss << " name \"" << record.name << "\"";
    }
    if(record.value.length() > 0){
        ss << " value \"" << record.value << "\"";
    }
    if(record.hash.length() > 0){
        ss << " hash " << shortKey(record.hash);
    }
    return ss.str();
}

void printMempoolRecords(){
    CFunctions functions;
    std::vector<CFunctions::record_structure> records = functions.peekQueueRecords();
    if(records.size() == 0){
        std::cout << " Mempool is empty." << std::endl;
        return;
    }

    std::cout << " Mempool records: " << records.size() << std::endl;
    for(int i = 0; i < records.size(); i++){
        std::cout << "  " << mempoolRecordSummary(i, records.at(i), true) << std::endl;
    }
}

void printRecentBlockchainRecords(int limit){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(firstBlockId < 0 || latestBlockId < 0){
        std::cout << " No blockchain records found." << std::endl;
        return;
    }

    std::deque<std::string> recentRecords;
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            recentRecords.push_back(recordSummary(block.number, i, block.records.at(i), false));
            if(recentRecords.size() > limit){
                recentRecords.pop_front();
            }
        }
        if(block.number == latestBlockId){
            break;
        }
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
        if(nextBlock.number <= 0 || nextBlock.number == block.number){
            break;
        }
        block = nextBlock;
        guard++;
    }

    if(recentRecords.size() == 0){
        std::cout << " No blockchain records found." << std::endl;
        return;
    }

    std::cout << " Last " << recentRecords.size() << " blockchain records:" << std::endl;
    for(std::deque<std::string>::iterator it = recentRecords.begin(); it != recentRecords.end(); ++it){
        std::cout << "  " << *it << std::endl;
    }
}

void printMembershipRecords(){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(firstBlockId < 0 || latestBlockId < 0){
        std::cout << " No blockchain records found." << std::endl;
        return;
    }

    int membershipCount = 0;
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.transaction_type == CFunctions::JOIN_NETWORK){
                if(membershipCount == 0){
                    std::cout << " Membership records:" << std::endl;
                }
                std::cout << "  " << recordSummary(block.number, i, record, true) << std::endl;
                membershipCount++;
            }
        }
        if(block.number == latestBlockId){
            break;
        }
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
        if(nextBlock.number <= 0 || nextBlock.number == block.number){
            break;
        }
        block = nextBlock;
        guard++;
    }

    std::cout << " Membership count: " << membershipCount << std::endl;
}

}

/**
* printCommands 
*
* Description: Print a list of commands that are accepted.
*/
void CCLI::printCommands(){
	std::cout << "Commands:\n" <<
	" join                    - request membership in the main network.\n" <<
    " switch [network name]   - switch current network. Default is 'main'.\n" <<
    //" swtch wallet            - change wallet" <<
	" balance                 - print balance and transaction summary.\n" <<
	" sent                    - print sent transaction list details.\n" <<
	" received                - print received transaction list details.\n" <<
	" network                 - print network stats including currency and volumes.\n" <<
    " mempool                 - print pending records not yet in the blockchain.\n" <<
    " nextblock               - print current and upcoming selected block creators.\n" <<
    " blockchain              - print the last 10 blockchain records.\n" <<
    " memberships             - print blockchain membership records.\n" <<
	" send                    - send a payment to another user address.\n" <<
	" receive                 - prints your public key address to have others send you payments.\n" <<
	" users                   - prints user address and balance information.\n" <<
    " vote                    - vote on network behaviour and settings.\n" <<
    " debug                   - log debug information.\n" <<
    " advanced                - more commands for admin and testing functions.\n" <<
	" quit                    - shutdown the application.\n" << std::endl;
}


void CCLI::printAdvancedCommands(){
    std::cout << " Advanced: \n" <<
    " reindex                - Clear and parse the entire blockchain dataset.\n" <<
    " tests                  - Run tests to verify this build is functioning correctly.\n" <<
    " chain                  - Scan the complete blockchain for verification. Reports findings.\n" <<
    " printchain             - Print the blockchain summary and validation.\n" <<
    " printqueue             - Print the local mempool queue.\n" <<
    " resetall               - Delete node data.\n" <<
    " requestblock           - Send network request for block data. \n" <<
    " sync                   - Pull blocks from configured local peers. \n" <<
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
    CRelayClient relayClient;
	std::string privateKey;
	std::string publicKey;
	CWallet wallet;
	bool e = wallet.fileExists("wallet.dat");
	if(e != 0){
		// Load wallet
		wallet.read(privateKey, publicKey);
		//functions.parseBlockFile(publicKey, false);
        functions.scanChain(publicKey, false);
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
                //functions.parseBlockFile(publicKey, false);
                functions.scanChain(publicKey, false);
            //}

            std::string networkName = "main";
            std::cout << "Enter public user name: \n" << std::endl;
            std::string userName;
            std::cin >> userName;

			if(functions.joined == true){ // TODO this needs to track different networks.
				std::cout << "Allready joined network. \n" << std::endl;
			} else {
				std::cout << "Joining request sending... \n" << std::endl;

				CFunctions::record_structure joinRecord;
				joinRecord.network = networkName;
				time_t  timev;
				time(&timev);
				std::stringstream ss;
				ss << timev;
				std::string ts = ss.str();
				joinRecord.time = ts;
				joinRecord.transaction_type = CFunctions::JOIN_NETWORK;
				joinRecord.amount = 0.0;
                joinRecord.fee = 0.0;
				joinRecord.sender_public_key = publicKey;
				joinRecord.recipient_public_key = "";
                joinRecord.name = userName;
				joinRecord.hash = functions.getRecordHash(joinRecord);
                std::string message_siganture = "";
				ecdsa.SignMessage(privateKey, joinRecord.hash, message_siganture);
				joinRecord.signature = message_siganture;	
				
                functions.addToQueue( joinRecord );
                relayClient.sendRecord(joinRecord);
                CLocalPeerClient::broadcastRecord(joinRecord);
                std::cout << "Join request queued and broadcast. Joined network will be yes after a block includes this record." << std::endl;
	
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
    
				//functions.parseBlockFile(publicKey, false);
                functions.scanChain(publicKey, false);
            
            // Print chain sync status.
            
            
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
                sendRecord.network = "main";
                time_t  timev;
                time(&timev);
                std::stringstream ss;
                ss << timev;
                std::string ts = ss.str();
                sendRecord.time = ts;
                sendRecord.transaction_type = CFunctions::TRANSFER_CURRENCY;
                sendRecord.amount = d_amount;
                sendRecord.fee = 0.0;
                sendRecord.sender_public_key = publicKey;
                sendRecord.recipient_public_key = destination_address;
                sendRecord.hash = functions.getRecordHash(sendRecord);
                std::string signature = "";
                ecdsa.SignMessage(privateKey, sendRecord.hash, signature);
                sendRecord.signature = signature;
                functions.addToQueue( sendRecord );
                relayClient.sendRecord(sendRecord);
                CLocalPeerClient::broadcastRecord(sendRecord);
                std::cout << "Sent transfer request. " << std::endl;
            }

        } else if ( command.find("network") != std::string::npos ){

            //functions.parseBlockFile(publicKey, false);
            functions.scanChain(publicKey, false);
            
            std::cout << " Network up to date: " << (functions.IsChainUpToDate() == true ? "yes" : "no ") << std::endl;
            if(CLocalPeerClient::getPeers().size() > 0){
                long peerLatestBlockId = CLocalPeerClient::getBestPeerLatestBlockId();
                std::cout << " Peer sync: " << (CLocalPeerClient::isSyncedWithPeers() == true ? "yes" : "no ") << std::endl;
                std::cout << " Peer latest block: " << peerLatestBlockId << std::endl;
            } else {
                std::cout << " Sync Progress: " << functions.SyncProgress() << "% " << std::endl;
            }
            CBlockDB networkBlockDB;
            std::cout << " First block: " << networkBlockDB.getFirstBlockId() << std::endl;
            std::cout << " Latest block: " << networkBlockDB.getLatestBlockId() << std::endl;
            
            std::cout << " Joined network: " << (functions.joined > 0 ? "yes" : "no") << std::endl;
            std::cout << " Your balance: " << functions.balance << " sfr" << std::endl;
                    std::cout << " Currency supply: " << functions.currency_circulation << " sfr" << std::endl;
                    std::cout << " User count: " << functions.user_count << std::endl;

            // Block chain size?
            std::cout << " Blockchain size: " << " 0MB" << std::endl;
        
            std::cout << " Pending transactions: " << " 0" << std::endl;
            
            // Active connections?
            //std::cout << "This feature is not implemented yet.\n" << std::endl;
            std::vector<CRelayClient::node_status> peers = relayClient.getPeers();
            std::cout << " Peers: " << peers.size() << std::endl;
            std::cout << " Local peers: " << CLocalPeerClient::getPeers().size() << std::endl;
            //for(int i = 0; i < peers.size(); i++){ std::cout << peers.at(i).public_key << " "; } std::cout << std::endl;
     
            //CP2P p2p;
            //std::cout << " Peer Address: " << p2p.myPeerAddress << std::endl;
                    //p2p.sendData("DATA DATA DATA 123 \0");

        } else if ( command.compare("blockchain") == 0 || command.compare("messages") == 0 ){

            printRecentBlockchainRecords(10);

        } else if ( command.compare("mempool") == 0 || command.compare("pending") == 0 ){

            printMempoolRecords();

        } else if ( command.compare("nextblock") == 0 ){

            printNextBlockSelection(publicKey);

        } else if ( command.compare("memberships") == 0 || command.compare("members") == 0 ){

            printMembershipRecords();

 
        } else if ( command.find("quit") != std::string::npos ){
                running = false;
            
            } else if ( command.find("advanced") != std::string::npos ){
                printAdvancedCommands();
            } else if ( command.find("tests") != std::string::npos ){
                CECDSACrypto ecdsa;
                ecdsa.runTests();



        } else if ( command.compare("chain") == 0){
           std::cout << " Blockchain state: " << " Not implemented. " << std::endl;
	
        } else if ( command.compare("printchain") == 0){
            
            // TODO: Only print issues and the last 5 blocks.
            // Otherwise it's too much data.
            
            std::cout << " Blockchain detail: " << std::endl;

            //functions.parseBlockFile(publicKey, true);
            functions.scanChain(publicKey, true);

            CBlockDB blockDB;
            //blockDB.GetBlocks();
            CFunctions::block_structure last_block = functions.getLastBlock("");
            CFunctions::block_structure block = blockDB.getBlock(last_block.number);
            
            long firstBlockId = blockDB.getFirstBlockId();
            long latestBlockId = blockDB.getLatestBlockId();
            std::cout << "First Block:  " << firstBlockId << std::endl;
            std::cout << "Latest Block: " << latestBlockId << std::endl;
            
            // long CBlockDB::getNextBlockId(long previousBlockId)


        } else if( command.compare("printqueue") == 0){
            printMempoolRecords();
            
        } else if( command.compare("resetall") == 0){
            std::cout << " Purging node data: " << std::endl;
            functions.DeleteAll();
            
            CBlockDB blockDB;
            blockDB.DeleteAll();
            
            // quit
            running = false;
            
        } else if( command.compare("reindex") == 0){
            std::cout << " Reindex: " << std::endl;
            
            CBlockDB blockDB;
            CUserDB userDB;
            userDB.DeleteIndex();
            
            functions.scanChain(publicKey, true); // debug true to show progress. perhaps add a progress bar/percentage output option later.
           
        } else if(command.compare("requestblock") == 0){
            std::cout << " Request block... " << std::endl;
          
            // What is the latest block this node has.
            CBlockDB blockDB;
            long latestBlockId = blockDB.getLatestBlockId();
            std::cout << " id: " << latestBlockId << std::endl;
            
            // Get connected nodes.
            CRelayClient relayClient;
            relayClient.sendRequestBlocks(latestBlockId);
            std::vector<std::string> localPeers = CLocalPeerClient::getPeers();
            for(int i = 0; i < localPeers.size(); i++){
                CLocalPeerClient::syncFromPeer(localPeers.at(i));
            }
            
        } else if(command.compare("sync") == 0){
            std::cout << " Syncing local peers... " << std::endl;
            std::vector<std::string> localPeers = CLocalPeerClient::getPeers();
            if(localPeers.size() == 0){
                std::cout << "  No local peers configured. Start with --peer http://host:port" << std::endl;
            }
            for(int i = 0; i < localPeers.size(); i++){
                std::cout << "  " << localPeers.at(i) << std::endl;
                CBlockDB syncBlockDB;
                long before = syncBlockDB.getLatestBlockId();
                long peerBefore = CLocalPeerClient::getPeerLatestBlockId(localPeers.at(i));
                CLocalPeerClient::syncFromPeer(localPeers.at(i));
                int pushed = CLocalPeerClient::pushToPeer(localPeers.at(i));
                long after = syncBlockDB.getLatestBlockId();
                long peerAfter = CLocalPeerClient::getPeerLatestBlockId(localPeers.at(i));
                std::cout << "    local latest: " << before << " -> " << after << std::endl;
                std::cout << "    peer latest: " << peerBefore << " -> " << peerAfter << std::endl;
                std::cout << "    pushed blocks: " << pushed << std::endl;
            }
            
        } else if(command.compare("users") == 0){
            std::cout << "Users: " << std::endl;
            
            CUserDB userDB;
            
            long count = userDB.getUserCount();
            std::cout << "User count: " << count << std::endl;
            
            std::vector<CFunctions::user_structure> users = userDB.getUsers();
            for(int i = 0; i < users.size(); i++){
                CFunctions::user_structure user = users.at(i);
                std::cout << "  user: " << user.public_key << " " << user.balance << " sfr " << std::endl;
            }
            /*
            for(int i = 0; i < functions.users.size(); i++){
                CFunctions::user_structure user = functions.users.at(i);
                std::cout << "  user: " << user.public_key << " " << user.balance << " sfr " << std::endl;
                // if current user publicKey mark
            }
             */
        } else if ( command.compare("vote") == 0){ 
             std::cout << " Block reward (min 0.1 - max 100): " << std::endl;
	
             std::string blockReward;
             std::cin >> blockReward;
             if(blockReward.compare("") == 0 ){
                 blockReward = "1";
             }
           
             // TEMP this record would be added to the queue and broadcast but for testing we just add it to the queue file.

             CFunctions::record_structure voteRecord;
             voteRecord.network = "main"; // networkName;
             time_t  timev;
             time(&timev);
             std::stringstream ss;
             ss << timev;
             std::string ts = ss.str();
             voteRecord.time = ts;
             CFunctions::transaction_types voteType = CFunctions::VOTE;
             voteRecord.transaction_type = voteType;
             voteRecord.name = "blockreward";
             voteRecord.value = blockReward;
             voteRecord.amount = 0.0;
             voteRecord.fee = 0;
             voteRecord.sender_public_key = publicKey;
             voteRecord.recipient_public_key = "";
             
             voteRecord.hash = functions.getRecordHash(voteRecord);
             std::string signature = "";
             ecdsa.SignMessage(privateKey, voteRecord.hash, signature);
             voteRecord.signature = signature;
 
             functions.addToQueue(voteRecord);
	     relayClient.sendRecord(voteRecord); 
	     CLocalPeerClient::broadcastRecord(voteRecord); 

		std::cout << "Vote sent." << std::endl;

	} else {
		printCommands();	
	}
    }
}
