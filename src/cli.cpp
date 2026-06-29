// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cli.h"

#include <cctype>
#include <cstdio>
#include <deque>
#include <map>
#include <sstream>
#include <unistd.h>   // open and close
#include <sys/stat.h> // temp because we removed util
#include <fcntl.h> // temp removed util.h
#include <time.h>
#ifndef _WIN32
#include <termios.h>
#endif
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "functions/selector.h"
#include "wallet.h"
#include "network/p2p.h"
#include "network/relayclient.h"
#include "network/localpeerclient.h"
#include "networktime.h"
#include "blockdb.h"
#include "userdb.h"

namespace {

std::string readCommandLine(std::string& lastCommand){
#ifndef _WIN32
    if(isatty(STDIN_FILENO)){
        termios oldTerm;
        termios newTerm;
        tcgetattr(STDIN_FILENO, &oldTerm);
        newTerm = oldTerm;
        newTerm.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);

        std::string command;
        bool done = false;
        while(done == false){
            int c = std::getchar();
            if(c == EOF){
                break;
            }
            if(c == '\n' || c == '\r'){
                std::cout << std::endl;
                done = true;
            } else if(c == 127 || c == 8){
                if(command.length() > 0){
                    command.erase(command.length() - 1);
                    std::cout << "\b \b" << std::flush;
                }
            } else if(c == 27){
                int c1 = std::getchar();
                int c2 = std::getchar();
                if(c1 == '[' && c2 == 'A' && lastCommand.length() > 0){
                    while(command.length() > 0){
                        command.erase(command.length() - 1);
                        std::cout << "\b \b";
                    }
                    command = lastCommand;
                    std::cout << command << std::flush;
                }
            } else if(std::isprint(c)){
                command.push_back(static_cast<char>(c));
                std::cout << static_cast<char>(c) << std::flush;
            }
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
        if(command.length() > 0){
            lastCommand = command;
        }
        return command;
    }
#endif

    std::string command;
    std::cin >> command;
    if(command.length() > 0){
        lastCommand = command;
    }
    return command;
}

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

std::string formatEpochTimeString(const std::string& epochString){
    if(epochString.length() == 0){
        return "-";
    }

    long epoch = ::atol(epochString.c_str());
    if(epoch <= 0){
        return epochString;
    }

    time_t recordTime = (time_t)epoch;
    struct tm * timeInfo = localtime(&recordTime);
    if(timeInfo == NULL){
        return epochString;
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
    return std::string(buffer);
}

std::string walletHistoryLine(
    const std::string& direction,
    long blockNumber,
    int recordIndex,
    CFunctions::record_structure record,
    const std::string& counterparty,
    double netAmount
){
    std::stringstream ss;
    ss << " " << direction;
    ss << " block " << blockNumber << " #" << recordIndex;
    ss << " time " << formatEpochTimeString(record.time);
    ss << " net " << netAmount << " sfr";
    ss << " amt " << record.amount;
    if(record.fee > 0){
        ss << " fee " << record.fee;
    }
    if(counterparty.length() > 0){
        ss << " with " << shortKey(counterparty);
    }
    if(record.name.length() > 0){
        ss << " note \"" << record.name << "\"";
    }
    if(record.hash.length() > 0){
        ss << " hash " << shortKey(record.hash);
    }
    return ss.str();
}

void addWalletHistoryRecord(
    std::deque<std::string>& history,
    int limit,
    const std::string& filter,
    const std::string& direction,
    long blockNumber,
    int recordIndex,
    CFunctions::record_structure record,
    const std::string& counterparty,
    double netAmount
){
    if(filter.compare("sent") == 0 && direction.compare("SENT") != 0 && direction.compare("SELF") != 0){
        return;
    }
    if(filter.compare("received") == 0 &&
       direction.compare("RECEIVED") != 0 &&
       direction.compare("REWARD") != 0 &&
       direction.compare("FEE") != 0 &&
       direction.compare("SELF") != 0){
        return;
    }

    history.push_back(walletHistoryLine(direction, blockNumber, recordIndex, record, counterparty, netAmount));
    if(history.size() > limit){
        history.pop_front();
    }
}

void printWalletHistory(const std::string& publicKey, const std::string& filter){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(firstBlockId < 0 || latestBlockId < 0){
        std::cout << " No blockchain records found." << std::endl;
        return;
    }

    const int limit = 25;
    std::deque<std::string> history;
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);

            if(record.transaction_type == CFunctions::ISSUE_CURRENCY &&
               record.recipient_public_key.compare(publicKey) == 0){
                addWalletHistoryRecord(history, limit, filter, "REWARD", block.number, i, record, block.creator_key, record.amount);
            }

            if(record.transaction_type == CFunctions::TRANSFER_CURRENCY){
                bool sentByWallet = record.sender_public_key.compare(publicKey) == 0;
                bool receivedByWallet = record.recipient_public_key.compare(publicKey) == 0;

                if(sentByWallet && receivedByWallet){
                    addWalletHistoryRecord(history, limit, filter, "SELF", block.number, i, record, publicKey, 0);
                } else if(sentByWallet){
                    addWalletHistoryRecord(history, limit, filter, "SENT", block.number, i, record, record.recipient_public_key, -(record.amount + record.fee));
                } else if(receivedByWallet){
                    addWalletHistoryRecord(history, limit, filter, "RECEIVED", block.number, i, record, record.sender_public_key, record.amount);
                }
            }

            if((record.transaction_type == CFunctions::TRANSFER_CURRENCY ||
                record.transaction_type == CFunctions::VOTE) &&
               block.creator_key.compare(publicKey) == 0 &&
               record.fee > 0){
                addWalletHistoryRecord(history, limit, filter, "FEE", block.number, i, record, record.sender_public_key, record.fee);
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

    if(history.size() == 0){
        std::cout << " No wallet history found." << std::endl;
        return;
    }

    std::string label = filter.length() > 0 ? filter : "wallet";
    std::cout << " Last " << history.size() << " " << label << " history records:" << std::endl;
    for(std::deque<std::string>::iterator it = history.begin(); it != history.end(); ++it){
        std::cout << "  " << *it << std::endl;
    }
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

void addBalanceDelta(std::map<std::string, double>& balances, const std::string& publicKey, double amount){
    if(publicKey.length() == 0){
        return;
    }
    balances[publicKey] += amount;
}

std::map<std::string, double> acceptedLedgerBalances(){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    std::map<std::string, double> balances;
    if(firstBlockId < 0 || latestBlockId < 0){
        return balances;
    }

    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.transaction_type == CFunctions::ISSUE_CURRENCY){
                addBalanceDelta(balances, record.recipient_public_key, record.amount);
            } else if(record.transaction_type == CFunctions::TRANSFER_CURRENCY){
                addBalanceDelta(balances, record.recipient_public_key, record.amount);
                addBalanceDelta(balances, record.sender_public_key, -record.amount - record.fee);
                addBalanceDelta(balances, block.creator_key, record.fee);
            } else if(record.transaction_type == CFunctions::VOTE){
                addBalanceDelta(balances, record.sender_public_key, -record.fee);
                addBalanceDelta(balances, block.creator_key, record.fee);
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
    return balances;
}

std::vector<CFunctions::record_structure> activeMembershipRecords(){
    CBlockDB blockDB;
    CSelector selector;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    std::vector<CFunctions::record_structure> members;
    std::vector<CFunctions::record_structure> activeMembers;
    std::map<std::string, long> latestHeartbeatBlockByUser;
    bool sawHeartbeat = false;
    if(firstBlockId < 0 || latestBlockId < 0){
        return activeMembers;
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
            if(record.transaction_type == CFunctions::HEART_BEAT &&
               record.sender_public_key.length() > 0){
                sawHeartbeat = true;
                latestHeartbeatBlockByUser[record.sender_public_key] = block.number;
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

    long heartbeatCutoff = selector.getCurrentTimeBlock() - CFunctions::HEARTBEAT_VALID_BLOCKS;
    for(int m = 0; m < members.size(); m++){
        std::string memberKey = members.at(m).sender_public_key;
        if(sawHeartbeat == false ||
           (latestHeartbeatBlockByUser.find(memberKey) != latestHeartbeatBlockByUser.end() &&
            latestHeartbeatBlockByUser[memberKey] >= heartbeatCutoff)){
            activeMembers.push_back(members.at(m));
        }
    }
    return activeMembers;
}

void printSelectedBlockCreator(const std::vector<CFunctions::record_structure>& members, long timeBlock, const std::string& label, const std::string& localPublicKey){
    std::cout << " " << label << " block: " << timeBlock << std::endl;
    std::cout << " Time: " << formatSlotTime(timeBlock) << std::endl;
    if(members.size() == 0){
        std::cout << " Creator: none selected; no active heartbeat members loaded." << std::endl;
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

void printNextLocalCreatorSlot(const std::vector<CFunctions::record_structure>& members, long startBlock, const std::string& localPublicKey){
    if(members.size() == 0 || localPublicKey.length() == 0){
        std::cout << " Next time this node is selected: unknown" << std::endl;
        return;
    }

    const long maxLookAheadBlocks = 100000;
    for(long offset = 0; offset <= maxLookAheadBlocks; offset++){
        long candidateBlock = startBlock + offset;
        long userIndex = candidateBlock % members.size();
        if(members.at(userIndex).sender_public_key.compare(localPublicKey) == 0){
            long secondsUntil = candidateBlock > startBlock ? offset * 15 : 0;
            double minutesUntil = secondsUntil / 60.0;
            std::cout << " Next time this node is selected:" << std::endl;
            std::cout << "  block: " << candidateBlock << std::endl;
            std::cout << "  time: " << formatSlotTime(candidateBlock) << std::endl;
            std::cout << "  approximately: " << minutesUntil << " minutes" << std::endl;
            return;
        }
    }

    std::cout << " Next time this node is selected: not found in lookahead window" << std::endl;
}

void printNextBlockSelection(const std::string& localPublicKey){
    CSelector selector;
    CNetworkTime netTime;
    long currentBlock = selector.getCurrentTimeBlock();
    long nextBlock = currentBlock + 1;
    std::vector<CFunctions::record_structure> acceptedMembers = acceptedMembershipRecords();
    std::vector<CFunctions::record_structure> members = activeMembershipRecords();
    long now = netTime.getEpoch();
    long secondsUntilNextBlock = (nextBlock * 15) - now;
    if(secondsUntilNextBlock < 0){
        secondsUntilNextBlock = 0;
    }

    std::cout << " Block creator selection:" << std::endl;
    std::cout << " Network time offset: " << netTime.getOffset() << "s" << std::endl;
    std::cout << " Accepted members: " << acceptedMembers.size() << std::endl;
    std::cout << " Active heartbeat members: " << members.size() << std::endl;
    printSelectedBlockCreator(members, currentBlock, "Current", localPublicKey);
    std::cout << " Seconds until next block: " << secondsUntilNextBlock << std::endl;
    printSelectedBlockCreator(members, nextBlock, "Next", localPublicKey);
    printNextLocalCreatorSlot(members, currentBlock, localPublicKey);
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
    long connectedBlockCount = 0;
    long totalRecordCount = 0;
    long connectedLatestBlockId = -1;
    while(block.number > 0 && guard < 100000){
        connectedBlockCount++;
        connectedLatestBlockId = block.number;
        for(int i = 0; i < block.records.size(); i++){
            totalRecordCount++;
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

    long slotSpan = connectedLatestBlockId > firstBlockId ? connectedLatestBlockId - firstBlockId : 0;
    long secondsSpan = slotSpan * 15;
    double daysSpan = secondsSpan / 86400.0;
    std::cout << " Blockchain stats:" << std::endl;
    std::cout << "  first block: " << firstBlockId << std::endl;
    std::cout << "  latest connected block: " << connectedLatestBlockId << std::endl;
    std::cout << "  latest indexed block: " << latestBlockId << std::endl;
    std::cout << "  connected blocks: " << connectedBlockCount << std::endl;
    std::cout << "  block span: " << slotSpan << " slots (~" << daysSpan << " days)" << std::endl;
    std::cout << "  records: " << totalRecordCount << std::endl;
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
    " heartbeat               - renew block creator eligibility.\n" <<
    " switch [network name]   - switch current network. Default is 'main'.\n" <<
    //" swtch wallet            - change wallet" <<
	" balance                 - print balance and transaction summary.\n" <<
    " history                 - print sent and received wallet history.\n" <<
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
    std::string lastCommand;
	while(running){
		std::cout << ">";		

		std::string command = readCommandLine(lastCommand);
        if(!std::cin && command.length() == 0){
            running = false;
            continue;
        }
        if(command.length() == 0){
            continue;
        }

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
                CNetworkTime netTime;
				std::stringstream ss;
				ss << netTime.getEpoch();
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
        } else if ( command.compare("heartbeat") == 0 ){
            functions.scanChain(publicKey, false);
            if(functions.joined == false){
                std::cout << " Join the network before sending a heartbeat." << std::endl;
            } else {
                CFunctions::record_structure heartbeatRecord;
                heartbeatRecord.network = "main";
                CNetworkTime netTime;
                std::stringstream ss;
                ss << netTime.getEpoch();
                heartbeatRecord.time = ss.str();
                heartbeatRecord.transaction_type = CFunctions::HEART_BEAT;
                heartbeatRecord.amount = 0.0;
                heartbeatRecord.fee = 0.0;
                heartbeatRecord.sender_public_key = publicKey;
                heartbeatRecord.recipient_public_key = "";
                heartbeatRecord.hash = functions.getRecordHash(heartbeatRecord);
                std::string signature = "";
                ecdsa.SignMessage(privateKey, heartbeatRecord.hash, signature);
                heartbeatRecord.signature = signature;

                functions.addToQueue(heartbeatRecord);
                relayClient.sendRecord(heartbeatRecord);
                CLocalPeerClient::broadcastRecord(heartbeatRecord);
                std::cout << "Heartbeat queued and broadcast. Eligibility renews after a block includes this record." << std::endl;
            }

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
        } else if ( command.compare("history") == 0 ){
            printWalletHistory(publicKey, "");

        } else if ( command.compare("sent") == 0 ){
            printWalletHistory(publicKey, "sent");
            
        } else if ( command.compare("received") == 0 ){
            std::cout << "Your receiving address is: " << publicKey << "\n" << std::endl;
            printWalletHistory(publicKey, "received");

        } else if ( command.compare("receive") == 0 ){
            
            std::cout << "Your receiving address is: " << publicKey << "\n" << std::endl;
 
        } else if ( command.compare("send") == 0 ){
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
                CNetworkTime netTime;
                std::stringstream ss;
                ss << netTime.getEpoch();
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

            CLocalPeerClient::syncNetworkTime();
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
            CNetworkTime networkTime;
            std::cout << " Network time offset: " << networkTime.getOffset() << "s" << std::endl;
            std::cout << " Clock status: " << (networkTime.isClockHealthy() ? "ok" : "check local clock") << std::endl;
            
            std::cout << " Joined network: " << (functions.joined > 0 ? "yes" : "no") << std::endl;
            std::vector<CFunctions::record_structure> activeMembers = activeMembershipRecords();
            bool activeHeartbeat = false;
            for(int i = 0; i < activeMembers.size(); i++){
                if(activeMembers.at(i).sender_public_key.compare(publicKey) == 0){
                    activeHeartbeat = true;
                }
            }
            std::cout << " Active heartbeat: " << (activeHeartbeat ? "yes" : "no") << std::endl;
            std::cout << " Heartbeat renewal due: " << (functions.heartbeat_renewal_due ? "yes" : "no") << std::endl;
            std::cout << " Last heartbeat block: ";
            if(functions.last_heartbeat_block > -1){
                std::cout << functions.last_heartbeat_block;
            } else {
                std::cout << "-";
            }
            std::cout << std::endl;
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

            CLocalPeerClient::syncNetworkTime();
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
            CLocalPeerClient::syncNetworkTime();
            CNetworkTime syncNetworkTime;
            std::cout << "  Network time offset: " << syncNetworkTime.getOffset() << "s" << std::endl;
            for(int i = 0; i < localPeers.size(); i++){
                std::cout << "  " << localPeers.at(i) << std::endl;
                CBlockDB syncBlockDB;
                long before = syncBlockDB.getLatestBlockId();
                long peerBefore = CLocalPeerClient::getPeerLatestBlockId(localPeers.at(i));
                CLocalPeerClient::syncFromPeer(localPeers.at(i));
                CLocalPeerClient::push_result pushResult = CLocalPeerClient::pushToPeerDetailed(localPeers.at(i));
                long after = syncBlockDB.getLatestBlockId();
                long peerAfter = CLocalPeerClient::getPeerLatestBlockId(localPeers.at(i));
                std::cout << "    local latest: " << before << " -> " << after << std::endl;
                std::cout << "    peer latest: " << peerBefore << " -> " << peerAfter << std::endl;
                std::cout << "    candidate blocks: " << pushResult.candidateBlocks << std::endl;
                std::cout << "    pushed blocks: " << pushResult.pushedBlocks << std::endl;
                if(pushResult.failedBlockId > -1){
                    std::cout << "    failed block: " << pushResult.failedBlockId << std::endl;
                    std::cout << "    response: " << (pushResult.response.length() > 0 ? pushResult.response : "(empty response)") << std::endl;
                }
            }
            
        } else if(command.compare("users") == 0){
            std::cout << "Users: " << std::endl;
            std::vector<CFunctions::record_structure> members = acceptedMembershipRecords();
            std::map<std::string, double> balances = acceptedLedgerBalances();

            std::cout << "User count: " << members.size() << std::endl;

            for(int i = 0; i < members.size(); i++){
                CFunctions::record_structure member = members.at(i);
                std::cout << "  user: " << member.sender_public_key << " " << balances[member.sender_public_key] << " sfr";
                if(member.name.length() > 0){
                    std::cout << " name \"" << member.name << "\"";
                }
                std::cout << std::endl;
            }
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
             CNetworkTime netTime;
             std::stringstream ss;
             ss << netTime.getEpoch();
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
