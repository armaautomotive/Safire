// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cli.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>
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
#include "networkconfig.h"
#include "networktime.h"
#include "blockdb.h"
#include "userdb.h"

namespace {

std::vector<CFunctions::record_structure> acceptedMembershipRecords();
double accountBalanceAtBlock(const std::string& accountPublicKey, long checkpointBlock);

const double DEFAULT_TRANSACTION_FEE = 0.0;
const double MAX_TRANSACTION_FEE = 0.1;
const char* SETTINGS_FILE = "settings.dat";

bool executableFileExists(const std::string& path){
    return access(path.c_str(), X_OK) == 0;
}

bool graphicalSessionAvailable(){
#ifdef __APPLE__
    return getenv("SSH_CONNECTION") == NULL && getenv("SSH_TTY") == NULL;
#elif defined(_WIN32)
    return true;
#else
    return getenv("DISPLAY") != NULL ||
        getenv("WAYLAND_DISPLAY") != NULL ||
        getenv("MIR_SOCKET") != NULL;
#endif
}

std::string shellQuote(const std::string& value){
    std::string quoted = "'";
    for(std::size_t i = 0; i < value.length(); i++){
        if(value[i] == '\''){
            quoted += "'\\''";
        } else {
            quoted += value[i];
        }
    }
    quoted += "'";
    return quoted;
}

std::string guiLauncherPath(){
    std::vector<std::string> candidates;
    candidates.push_back("gui_client/run.sh");
    candidates.push_back("./gui_client/run.sh");
    candidates.push_back("../gui_client/run.sh");

    for(int i = 0; i < candidates.size(); i++){
        if(executableFileExists(candidates.at(i))){
            return candidates.at(i);
        }
    }
    return "";
}

bool launchGuiInterface(std::string& message){
    if(graphicalSessionAvailable() == false){
        message = "No graphical window session detected. Start the GUI from a desktop session.";
        return false;
    }

    std::string launcher = guiLauncherPath();
    if(launcher.length() == 0){
        message = "GUI launcher not found. Build the GUI with scripts/gui.sh first.";
        return false;
    }

    std::string command = shellQuote(launcher) + " >/tmp/safire-gui.log 2>&1 &";
    int result = system(command.c_str());
    if(result != 0){
        message = "GUI launch command failed. See /tmp/safire-gui.log for details.";
        return false;
    }

    message = "GUI launch requested.";
    return true;
}

std::string firstCommandToken(const std::string& command){
    std::istringstream ss(command);
    std::string token;
    ss >> token;
    return token;
}

std::string commandArgument(const std::string& command){
    std::size_t start = command.find_first_of(" \t");
    if(start == std::string::npos){
        return "";
    }
    start = command.find_first_not_of(" \t", start);
    if(start == std::string::npos){
        return "";
    }
    return command.substr(start);
}

bool parseFeeAmount(const std::string& value, double& fee){
    char* end = NULL;
    fee = std::strtod(value.c_str(), &end);
    if(end == value.c_str() || *end != '\0'){
        return false;
    }
    return fee >= 0.0 && fee <= MAX_TRANSACTION_FEE;
}

double getDefaultTransactionFee(){
    std::ifstream infile(SETTINGS_FILE);
    std::string line;
    while(std::getline(infile, line)){
        std::size_t start = line.find("fee:");
        if(start == 0){
            std::string value = line.substr(4);
            double fee = DEFAULT_TRANSACTION_FEE;
            if(parseFeeAmount(value, fee)){
                return fee;
            }
        }
    }
    return DEFAULT_TRANSACTION_FEE;
}

bool setDefaultTransactionFee(double fee){
    if(fee < 0.0 || fee > MAX_TRANSACTION_FEE){
        return false;
    }
    std::ofstream outfile(SETTINGS_FILE);
    if(!outfile.good()){
        return false;
    }
    outfile << "fee:" << fee << "\n";
    outfile.close();
    return true;
}

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
    if(type == CFunctions::UPDATE_NAME){
        return "UPDATE_NAME";
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

std::string normalizedPublicName(const std::string& name){
    if(name.length() < 3 || name.length() > 32){
        return "";
    }

    std::string normalized;
    for(int i = 0; i < name.length(); i++){
        unsigned char ch = static_cast<unsigned char>(name.at(i));
        if(std::isalnum(ch)){
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        } else if(ch == '-' || ch == '_'){
            normalized.push_back(static_cast<char>(ch));
        } else {
            return "";
        }
    }
    return normalized;
}

bool isPublicNameValid(const std::string& name){
    return normalizedPublicName(name).length() > 0;
}

int memberIndexByKey(const std::vector<CFunctions::record_structure>& members, const std::string& publicKey){
    for(int i = 0; i < members.size(); i++){
        if(members.at(i).sender_public_key.compare(publicKey) == 0){
            return i;
        }
    }
    return -1;
}

bool claimPublicName(std::vector<CFunctions::record_structure>& members, std::map<std::string, std::string>& nameOwners, const std::string& publicKey, const std::string& name){
    std::string normalized = normalizedPublicName(name);
    if(normalized.length() == 0){
        return false;
    }

    int memberIndex = memberIndexByKey(members, publicKey);
    if(memberIndex < 0){
        return false;
    }

    std::map<std::string, std::string>::iterator owner = nameOwners.find(normalized);
    if(owner != nameOwners.end() && owner->second.compare(publicKey) != 0){
        return false;
    }

    std::string previousName = normalizedPublicName(members.at(memberIndex).name);
    if(previousName.length() > 0){
        std::map<std::string, std::string>::iterator previousOwner = nameOwners.find(previousName);
        if(previousOwner != nameOwners.end() && previousOwner->second.compare(publicKey) == 0){
            nameOwners.erase(previousOwner);
        }
    }

    members[memberIndex].name = name;
    nameOwners[normalized] = publicKey;
    return true;
}

bool publicNameAvailableForOwner(const std::string& name, const std::string& ownerPublicKey){
    std::string normalized = normalizedPublicName(name);
    if(normalized.length() == 0){
        return false;
    }

    std::vector<CFunctions::record_structure> members = acceptedMembershipRecords();
    for(int i = 0; i < members.size(); i++){
        std::string memberName = normalizedPublicName(members.at(i).name);
        if(memberName.compare(normalized) == 0 &&
           members.at(i).sender_public_key.compare(ownerPublicKey) != 0){
            return false;
        }
    }
    return true;
}

std::map<std::string, std::string> acceptedMemberNames(){
    std::map<std::string, std::string> names;
    std::vector<CFunctions::record_structure> members = acceptedMembershipRecords();
    for(int i = 0; i < members.size(); i++){
        CFunctions::record_structure member = members.at(i);
        if(member.sender_public_key.length() > 0 && member.name.length() > 0){
            names[member.sender_public_key] = member.name;
        }
    }
    return names;
}

std::string namedAddress(const std::string& publicKey, const std::map<std::string, std::string>& names){
    if(publicKey.length() == 0){
        return "-";
    }

    std::map<std::string, std::string>::const_iterator it = names.find(publicKey);
    if(it != names.end() && it->second.length() > 0){
        return it->second + " (" + publicKey + ")";
    }

    return publicKey;
}

std::string walletHistoryLine(
    const std::string& direction,
    long blockNumber,
    int recordIndex,
    CFunctions::record_structure record,
    const std::string& fromKey,
    const std::string& toKey,
    double netAmount,
    const std::map<std::string, std::string>& names
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
    if(record.name.length() > 0){
        ss << " note \"" << record.name << "\"";
    }
    if(record.hash.length() > 0){
        ss << " hash " << shortKey(record.hash);
    }
    ss << "\n     from: " << namedAddress(fromKey, names);
    ss << "\n       to: " << namedAddress(toKey, names);
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
    const std::string& fromKey,
    const std::string& toKey,
    double netAmount,
    const std::map<std::string, std::string>& names
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

    history.push_back(walletHistoryLine(direction, blockNumber, recordIndex, record, fromKey, toKey, netAmount, names));
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
    std::map<std::string, std::string> names = acceptedMemberNames();
    std::set<std::string> acceptedRecordHashes;
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.hash.length() > 0 && acceptedRecordHashes.find(record.hash) != acceptedRecordHashes.end()){
                continue;
            }
            if(record.hash.length() > 0){
                acceptedRecordHashes.insert(record.hash);
            }

            if(record.transaction_type == CFunctions::ISSUE_CURRENCY &&
               record.recipient_public_key.compare(publicKey) == 0){
                addWalletHistoryRecord(history, limit, filter, "REWARD", block.number, i, record, record.sender_public_key, record.recipient_public_key, record.amount, names);
            }

            if(record.transaction_type == CFunctions::TRANSFER_CURRENCY){
                bool sentByWallet = record.sender_public_key.compare(publicKey) == 0;
                bool receivedByWallet = record.recipient_public_key.compare(publicKey) == 0;

                if(sentByWallet && receivedByWallet){
                    addWalletHistoryRecord(history, limit, filter, "SELF", block.number, i, record, publicKey, publicKey, 0, names);
                } else if(sentByWallet){
                    addWalletHistoryRecord(history, limit, filter, "SENT", block.number, i, record, record.sender_public_key, record.recipient_public_key, -(record.amount + record.fee), names);
                } else if(receivedByWallet){
                    addWalletHistoryRecord(history, limit, filter, "RECEIVED", block.number, i, record, record.sender_public_key, record.recipient_public_key, record.amount, names);
                }
            }

            if((record.transaction_type == CFunctions::TRANSFER_CURRENCY ||
                record.transaction_type == CFunctions::VOTE) &&
               block.creator_key.compare(publicKey) == 0 &&
               record.fee > 0){
                addWalletHistoryRecord(history, limit, filter, "FEE", block.number, i, record, record.sender_public_key, block.creator_key, record.fee, names);
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
    std::map<std::string, std::string> nameOwners;
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
                    std::string requestedName = record.name;
                    record.name = "";
                    members.push_back(record);
                    if(requestedName.length() > 0){
                        claimPublicName(members, nameOwners, record.sender_public_key, requestedName);
                    }
                }
            }
            if(record.transaction_type == CFunctions::UPDATE_NAME &&
               record.sender_public_key.length() > 0 &&
               record.name.length() > 0){
                claimPublicName(members, nameOwners, record.sender_public_key, record.name);
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

bool queueAndBroadcastRecord(CFunctions& functions, CRelayClient& relayClient, CFunctions::record_structure record){
    if(functions.isRecordSizeValid(record) == false){
        std::cout << " Record not sent: " << functions.recordSizeError(record) << "." << std::endl;
        return false;
    }
    if(functions.addToQueue(record) == 0){
        std::cout << " Record not sent: unable to queue record." << std::endl;
        return false;
    }
    relayClient.sendRecord(record);
    CLocalPeerClient::broadcastRecord(record);
    return true;
}

long parseCarryForwardValueLong(const std::string& value, const std::string& key){
    std::string prefix = key + "=";
    std::size_t start = value.find(prefix);
    if(start == std::string::npos){
        return -1;
    }
    start += prefix.length();
    std::size_t end = value.find(";", start);
    std::string section = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if(section.length() == 0){
        return -1;
    }
    return ::atol(section.c_str());
}

std::string carryForwardValue(long checkpointBlock, long period){
    std::stringstream ss;
    ss << "checkpoint=" << checkpointBlock << ";period=" << period << ";reward=" << CFunctions::CARRY_FORWARD_REWARD;
    return ss.str();
}

std::string carryForwardUniqueKey(const CFunctions::record_structure& record){
    long period = parseCarryForwardValueLong(record.value, "period");
    if(record.recipient_public_key.length() == 0 || period < 0){
        return "";
    }

    std::stringstream ss;
    ss << record.recipient_public_key << ":" << period;
    return ss.str();
}

bool isBasicCarryForwardRecord(const CFunctions::record_structure& record, long blockNumber){
    if(record.transaction_type != CFunctions::CARRY_FORWARD){
        return false;
    }
    if(record.sender_public_key.length() == 0 || record.recipient_public_key.length() == 0){
        return false;
    }
    long checkpoint = parseCarryForwardValueLong(record.value, "checkpoint");
    long period = parseCarryForwardValueLong(record.value, "period");
    if(checkpoint < 0 || period < 0 || checkpoint >= blockNumber){
        return false;
    }
    if(record.amount < 0){
        return false;
    }
    return true;
}

bool carryForwardSnapshotMatches(const CFunctions::record_structure& record){
    long checkpoint = parseCarryForwardValueLong(record.value, "checkpoint");
    if(checkpoint < 0){
        return false;
    }

    double expectedBalance = accountBalanceAtBlock(record.recipient_public_key, checkpoint);
    return std::fabs(expectedBalance - record.amount) < 0.000001;
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
    std::map<std::string, bool> acceptedCarryForwardKeys;
    std::set<std::string> acceptedRecordHashes;
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.hash.length() > 0 && acceptedRecordHashes.find(record.hash) != acceptedRecordHashes.end()){
                continue;
            }
            if(record.hash.length() > 0){
                acceptedRecordHashes.insert(record.hash);
            }
            if(record.transaction_type == CFunctions::ISSUE_CURRENCY){
                addBalanceDelta(balances, record.recipient_public_key, record.amount);
            } else if(record.transaction_type == CFunctions::TRANSFER_CURRENCY){
                addBalanceDelta(balances, record.recipient_public_key, record.amount);
                addBalanceDelta(balances, record.sender_public_key, -record.amount - record.fee);
                addBalanceDelta(balances, block.creator_key, record.fee);
            } else if(record.transaction_type == CFunctions::VOTE){
                addBalanceDelta(balances, record.sender_public_key, -record.fee);
                addBalanceDelta(balances, block.creator_key, record.fee);
            } else if(isBasicCarryForwardRecord(record, block.number) && carryForwardSnapshotMatches(record)){
                std::string key = carryForwardUniqueKey(record);
                if(key.length() > 0 && acceptedCarryForwardKeys.find(key) == acceptedCarryForwardKeys.end()){
                    acceptedCarryForwardKeys[key] = true;
                    addBalanceDelta(balances, record.sender_public_key, CFunctions::CARRY_FORWARD_REWARD);
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
    return balances;
}

double accountBalanceAtBlock(const std::string& accountPublicKey, long checkpointBlock){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(accountPublicKey.length() == 0 || firstBlockId < 0 || latestBlockId < 0){
        return 0.0;
    }

    double balance = 0.0;
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    std::map<std::string, bool> acceptedCarryForwardKeys;
    std::set<std::string> acceptedRecordHashes;
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        if(block.number > checkpointBlock){
            break;
        }
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.hash.length() > 0 && acceptedRecordHashes.find(record.hash) != acceptedRecordHashes.end()){
                continue;
            }
            if(record.hash.length() > 0){
                acceptedRecordHashes.insert(record.hash);
            }
            if(record.transaction_type == CFunctions::ISSUE_CURRENCY &&
               record.recipient_public_key.compare(accountPublicKey) == 0){
                balance += record.amount;
            } else if(record.transaction_type == CFunctions::TRANSFER_CURRENCY){
                if(record.recipient_public_key.compare(accountPublicKey) == 0){
                    balance += record.amount;
                }
                if(record.sender_public_key.compare(accountPublicKey) == 0 &&
                   record.recipient_public_key.compare(accountPublicKey) != 0){
                    balance -= record.amount;
                    balance -= record.fee;
                }
                if(block.creator_key.compare(accountPublicKey) == 0){
                    balance += record.fee;
                }
            } else if(record.transaction_type == CFunctions::VOTE){
                if(record.sender_public_key.compare(accountPublicKey) == 0){
                    balance -= record.fee;
                }
                if(block.creator_key.compare(accountPublicKey) == 0){
                    balance += record.fee;
                }
            } else if(isBasicCarryForwardRecord(record, block.number)){
                std::string key = carryForwardUniqueKey(record);
                if(key.length() > 0 && acceptedCarryForwardKeys.find(key) == acceptedCarryForwardKeys.end()){
                    acceptedCarryForwardKeys[key] = true;
                    if(record.sender_public_key.compare(accountPublicKey) == 0){
                        balance += CFunctions::CARRY_FORWARD_REWARD;
                    }
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
    return balance;
}

bool acceptedCarryForwardExists(const std::string& accountPublicKey, long period){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(accountPublicKey.length() == 0 || firstBlockId < 0 || latestBlockId < 0){
        return false;
    }

    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.transaction_type == CFunctions::CARRY_FORWARD &&
               record.recipient_public_key.compare(accountPublicKey) == 0 &&
               parseCarryForwardValueLong(record.value, "period") == period &&
               isBasicCarryForwardRecord(record, block.number) &&
               carryForwardSnapshotMatches(record)){
                return true;
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
    return false;
}

std::vector<CFunctions::record_structure> activeMembershipRecords(){
    CBlockDB blockDB;
    CSelector selector;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    std::vector<CFunctions::record_structure> members;
    std::vector<CFunctions::record_structure> activeMembers;
    std::map<std::string, long> latestHeartbeatBlockByUser;
    std::map<std::string, std::string> nameOwners;
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
                    std::string requestedName = record.name;
                    record.name = "";
                    members.push_back(record);
                    if(requestedName.length() > 0){
                        claimPublicName(members, nameOwners, record.sender_public_key, requestedName);
                    }
                }
            }
            if(record.transaction_type == CFunctions::UPDATE_NAME &&
               record.sender_public_key.length() > 0 &&
               record.name.length() > 0){
                claimPublicName(members, nameOwners, record.sender_public_key, record.name);
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
    if(record.transaction_type == CFunctions::JOIN_NETWORK || record.transaction_type == CFunctions::UPDATE_NAME){
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
    if(record.transaction_type == CFunctions::JOIN_NETWORK || record.transaction_type == CFunctions::UPDATE_NAME){
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

std::string blockSummary(CFunctions::block_structure block){
    std::stringstream ss;
    ss << "block " << block.number;
    ss << " prev " << block.previous_block_id;
    ss << " records " << block.records.size();
    ss << " creator " << shortKey(block.creator_key);
    if(block.hash.length() > 0){
        ss << " hash " << shortKey(block.hash);
    }
    if(block.previous_block_hash.length() > 0){
        ss << " prevhash " << shortKey(block.previous_block_hash);
    }
    return ss.str();
}

void printChainDiagnostics(){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    long connectedLatestBlockId = blockDB.getConnectedLatestBlockId();
    long connectedBlockCount = 0;
    CFunctions::block_structure connectedBlock = blockDB.getBlock(firstBlockId);
    int connectedGuard = 0;
    while(connectedBlock.number > 0 && connectedGuard < 100000){
        connectedBlockCount++;
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(connectedBlock);
        if(nextBlock.number <= 0 || nextBlock.number == connectedBlock.number){
            break;
        }
        connectedBlock = nextBlock;
        connectedGuard++;
    }
    std::vector<CFunctions::block_structure> storedBlocks = blockDB.getStoredBlocks();
    std::sort(storedBlocks.begin(), storedBlocks.end(),
        [](const CFunctions::block_structure& a, const CFunctions::block_structure& b){
            return a.number < b.number;
        });

    std::cout << " Chain diagnostics:" << std::endl;
    std::cout << "  first block: " << firstBlockId << std::endl;
    std::cout << "  latest indexed block: " << latestBlockId << std::endl;
    std::cout << "  latest connected block: " << connectedLatestBlockId << std::endl;
    std::cout << "  connected blocks: " << connectedBlockCount << std::endl;
    std::cout << "  stored blocks: " << storedBlocks.size() << std::endl;

    if(connectedLatestBlockId < 0){
        return;
    }

    CFunctions::block_structure connectedTip = blockDB.getBlock(connectedLatestBlockId);
    std::cout << "  connected tip: " << blockSummary(connectedTip) << std::endl;

    long nextIndex = blockDB.getNextBlockId(connectedLatestBlockId);
    std::cout << "  indexed child after connected tip: ";
    if(nextIndex > 0){
        std::cout << nextIndex;
    } else {
        std::cout << "-";
    }
    std::cout << std::endl;
    if(nextIndex > 0){
        CFunctions::block_structure nextBlock = blockDB.getBlock(nextIndex);
        if(nextBlock.number > 0){
            std::cout << "  indexed child block: " << blockSummary(nextBlock) << std::endl;
        } else {
            std::cout << "  indexed child block: missing" << std::endl;
        }
    }

    std::vector<CFunctions::block_structure> directChildren;
    std::vector<CFunctions::block_structure> storedAfterTip;
    for(int i = 0; i < storedBlocks.size(); i++){
        CFunctions::block_structure block = storedBlocks.at(i);
        if(block.previous_block_id == connectedLatestBlockId){
            directChildren.push_back(block);
        }
        if(block.number > connectedLatestBlockId){
            storedAfterTip.push_back(block);
        }
    }

    std::cout << "  direct stored children of connected tip: " << directChildren.size() << std::endl;
    for(int i = 0; i < directChildren.size() && i < 10; i++){
        std::cout << "    " << blockSummary(directChildren.at(i)) << std::endl;
    }

    std::cout << "  stored blocks after connected tip: " << storedAfterTip.size() << std::endl;
    for(int i = 0; i < storedAfterTip.size() && i < 10; i++){
        CFunctions::block_structure block = storedAfterTip.at(i);
        long parentNextIndex = blockDB.getNextBlockId(block.previous_block_id);
        std::cout << "    " << blockSummary(block);
        if(parentNextIndex > 0 && parentNextIndex != block.number){
            std::cout << " parent-index " << parentNextIndex;
        }
        std::cout << std::endl;
    }
}

void printChainIdentity(){
    CNetworkConfig config = CNetworkConfig::load();
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    CFunctions::block_structure firstBlock = blockDB.getBlock(firstBlockId);

    std::cout << " Chain identity:" << std::endl;
    std::cout << "  network: " << config.network << std::endl;
    std::cout << "  configured genesis block: " << config.genesisBlock << std::endl;
    std::cout << "  configured genesis hash: " << (config.genesisHash.length() > 0 ? config.genesisHash : "-") << std::endl;
    std::cout << "  strict genesis: " << (config.strictGenesis ? "yes" : "no") << std::endl;
    std::cout << "  local genesis block: " << firstBlockId << std::endl;
    std::cout << "  local genesis hash: " << (firstBlock.hash.length() > 0 ? firstBlock.hash : "-") << std::endl;
    std::cout << "  matches configured genesis: " << (config.genesisMatches(firstBlockId, firstBlock.hash) ? "yes" : "no") << std::endl;
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

void printCarryForwardRecords(){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(firstBlockId < 0 || latestBlockId < 0){
        std::cout << " No blockchain records found." << std::endl;
        return;
    }

    int carryForwardCount = 0;
    int validCarryForwardCount = 0;
    std::map<std::string, bool> acceptedCarryForwardKeys;
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.transaction_type == CFunctions::CARRY_FORWARD){
                if(carryForwardCount == 0){
                    std::cout << " Carry-forward records:" << std::endl;
                }
                bool valid = isBasicCarryForwardRecord(record, block.number) && carryForwardSnapshotMatches(record);
                std::string key = carryForwardUniqueKey(record);
                bool duplicate = key.length() > 0 && acceptedCarryForwardKeys.find(key) != acceptedCarryForwardKeys.end();
                if(valid && duplicate == false){
                    acceptedCarryForwardKeys[key] = true;
                    validCarryForwardCount++;
                }
                std::cout << "  " << recordSummary(block.number, i, record, true);
                std::cout << " status " << (valid && duplicate == false ? "accepted" : "ignored");
                if(duplicate){
                    std::cout << " duplicate-period";
                }
                std::cout << std::endl;
                carryForwardCount++;
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

    std::cout << " Carry-forward count: " << carryForwardCount << std::endl;
    std::cout << " Accepted carry-forwards: " << validCarryForwardCount << std::endl;
    std::cout << " Carry-forward reward: " << CFunctions::CARRY_FORWARD_REWARD << " sfr" << std::endl;
    std::cout << " Carry-forward period: " << CFunctions::CARRY_FORWARD_PERIOD_BLOCKS << " blocks" << std::endl;
    std::cout << " Prune horizon: " << CFunctions::CARRY_FORWARD_PRUNE_BLOCKS << " blocks" << std::endl;
}

void printVoteRecords(){
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if(firstBlockId < 0 || latestBlockId < 0){
        std::cout << " No blockchain records found." << std::endl;
        return;
    }

    int voteCount = 0;
    std::map<std::string, std::map<std::string, int> > tallies;
    std::map<std::string, std::string> names = acceptedMemberNames();
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while(block.number > 0 && guard < 100000){
        for(int i = 0; i < block.records.size(); i++){
            CFunctions::record_structure record = block.records.at(i);
            if(record.transaction_type == CFunctions::VOTE){
                if(voteCount == 0){
                    std::cout << " Vote records:" << std::endl;
                }
                std::string voteName = record.name.length() > 0 ? record.name : "-";
                std::string voteValue = record.value.length() > 0 ? record.value : "-";
                std::cout << "  block " << block.number << " #" << i;
                std::cout << " time " << (record.time.length() > 0 ? record.time : "-");
                std::cout << " name \"" << voteName << "\"";
                std::cout << " value \"" << voteValue << "\"";
                std::cout << " voter " << namedAddress(record.sender_public_key, names);
                if(record.hash.length() > 0){
                    std::cout << " hash " << shortKey(record.hash);
                }
                std::cout << std::endl;
                tallies[voteName][voteValue]++;
                voteCount++;
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

    std::cout << " Vote count: " << voteCount << std::endl;
    if(voteCount > 0){
        std::cout << " Vote tallies:" << std::endl;
        for(std::map<std::string, std::map<std::string, int> >::iterator topic = tallies.begin(); topic != tallies.end(); ++topic){
            std::cout << "  " << topic->first << std::endl;
            for(std::map<std::string, int>::iterator value = topic->second.begin(); value != topic->second.end(); ++value){
                std::cout << "    " << value->first << ": " << value->second << std::endl;
            }
        }
    }
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
    " setname                 - update your public member name.\n" <<
    " heartbeat               - renew block creator eligibility.\n" <<
    " switch [network name]   - switch current network. Default is 'main'.\n" <<
    //" swtch wallet            - change wallet" <<
	" balance                 - print balance and transaction summary.\n" <<
    " fee                     - print default transaction fee.\n" <<
    " setfee [amount]         - set default transaction fee for sends.\n" <<
    " history                 - print sent and received wallet history.\n" <<
	" sent                    - print sent transaction list details.\n" <<
	" received                - print received transaction list details.\n" <<
	" network                 - print network stats including currency and volumes.\n" <<
    " mempool                 - print pending records not yet in the blockchain.\n" <<
    " nextblock               - print current and upcoming selected block creators.\n" <<
    " blockchain              - print the last 10 blockchain records.\n" <<
    " memberships             - print blockchain membership records.\n" <<
    " carryforward            - submit a carry-forward candidate for this wallet.\n" <<
    " carryforwards           - print accepted carry-forward records.\n" <<
	" send                    - send a payment to another user address.\n" <<
	" receive                 - prints your public key address to have others send you payments.\n" <<
	" users                   - prints user address and balance information.\n" <<
    " peers                   - print peer discovery and health information.\n" <<
    " vote                    - submit a signed vote with a name and value.\n" <<
    " votes                   - print accepted vote submissions and tallies.\n" <<
    " gui                     - launch the graphical interface when available.\n" <<
    " debug                   - log debug information.\n" <<
    " advanced                - more commands for admin and testing functions.\n" <<
	" quit                    - shutdown the application.\n" << std::endl;
}


void CCLI::printAdvancedCommands(){
    std::cout << " Advanced: \n" <<
    " reindex                - Clear and parse the entire blockchain dataset.\n" <<
    " tests                  - Run tests to verify this build is functioning correctly.\n" <<
    " chainid                - Print configured and local genesis identity.\n" <<
    " chain                  - Scan the complete blockchain for verification. Reports findings.\n" <<
    " repairchain            - Rebuild next-block indexes using the best stored branch.\n" <<
    " printchain             - Print the blockchain summary and validation.\n" <<
    " printqueue             - Print the local mempool queue.\n" <<
    " resetall               - Delete node data.\n" <<
    " requestblock           - Send network request for block data. \n" <<
    " sync                   - Pull blocks from configured local peers. \n" <<
    " syncfull               - Push the full connected chain to local peers. \n" <<
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

            if(isPublicNameValid(userName) == false){
                std::cout << "Name not accepted. Use 3-32 characters: letters, numbers, dash, or underscore." << std::endl;
            } else if(publicNameAvailableForOwner(userName, publicKey) == false){
                std::cout << "Name not accepted. That public name is already taken." << std::endl;
			} else if(functions.joined == true){ // TODO this needs to track different networks.
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
				
                if(queueAndBroadcastRecord(functions, relayClient, joinRecord)){
                    std::cout << "Join request queued and broadcast. Joined network will be yes after a block includes this record." << std::endl;
                }
	
				// TODO: send request or say allready sent. 	
			}

			// Print if pending or allready accepted.
        } else if ( command.compare("setname") == 0 ){
            functions.scanChain(publicKey, false);
            if(functions.joined == false){
                std::cout << " Join the network before updating your public name." << std::endl;
            } else {
                std::cout << "Enter new public user name: \n" << std::endl;
                std::string userName;
                std::cin >> userName;
                if(isPublicNameValid(userName) == false){
                    std::cout << "Name update not sent. Use 3-32 characters: letters, numbers, dash, or underscore." << std::endl;
                } else if(publicNameAvailableForOwner(userName, publicKey) == false){
                    std::cout << "Name update not sent. That public name is already taken." << std::endl;
                } else {
                    CFunctions::record_structure nameRecord;
                    nameRecord.network = "main";
                    CNetworkTime netTime;
                    std::stringstream ss;
                    ss << netTime.getEpoch();
                    nameRecord.time = ss.str();
                    nameRecord.transaction_type = CFunctions::UPDATE_NAME;
                    nameRecord.amount = 0.0;
                    nameRecord.fee = 0.0;
                    nameRecord.sender_public_key = publicKey;
                    nameRecord.recipient_public_key = "";
                    nameRecord.name = userName;
                    nameRecord.hash = functions.getRecordHash(nameRecord);
                    std::string signature = "";
                    ecdsa.SignMessage(privateKey, nameRecord.hash, signature);
                    nameRecord.signature = signature;

                    if(queueAndBroadcastRecord(functions, relayClient, nameRecord)){
                        std::cout << "Name update queued and broadcast. The new name will show after a block includes this record." << std::endl;
                    }
                }
            }
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

                if(queueAndBroadcastRecord(functions, relayClient, heartbeatRecord)){
                    std::cout << "Heartbeat queued and broadcast. Eligibility renews after a block includes this record." << std::endl;
                }
            }

		} else if ( command.compare("fee") == 0 ){
            std::cout << " Default transaction fee: " << getDefaultTransactionFee() << " sfr" << std::endl;

        } else if ( firstCommandToken(command).compare("setfee") == 0 ){
            std::string feeValue = commandArgument(command);
            if(feeValue.length() == 0){
                std::cout << "Enter default transaction fee: \n" << std::endl;
                std::cin >> feeValue;
            }

            double fee = 0.0;
            if(parseFeeAmount(feeValue, fee) == false){
                std::cout << " Fee not changed. Use a number from 0 to " << MAX_TRANSACTION_FEE << " sfr." << std::endl;
            } else if(setDefaultTransactionFee(fee) == false){
                std::cout << " Fee not changed. Unable to write settings." << std::endl;
            } else {
                std::cout << " Default transaction fee set to " << fee << " sfr." << std::endl;
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
            functions.scanChain(publicKey, false);
            std::cout << "Enter destination address: \n" << std::endl;
            std::string destination_address;
            std::cin >> destination_address;
            std::cout << "Enter amount to send: \n" << std::endl;
            std::string amount;
            std::cin >> amount;
            std::cout << "Sending " << amount << " to user: " << destination_address << " \n" << std::endl;

            double d_amount = ::atof(amount.c_str());
            double transactionFee = getDefaultTransactionFee();
            double totalDebit = d_amount + transactionFee;

            // TODO also check balance adjusted using sent requests in queue....
            // TODO check destination address is an accepted user
            if(d_amount <= 0.0){
                std::cout << "Invalid amount. Unable to send transfer request. " << std::endl;
            } else if(totalDebit > functions.balance ){
                std::cout << "Insuficient balance. Unable to send transfer request. Amount plus fee is " << totalDebit << " sfr." << std::endl;
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
                sendRecord.fee = transactionFee;
                sendRecord.sender_public_key = publicKey;
                sendRecord.recipient_public_key = destination_address;
                sendRecord.hash = functions.getRecordHash(sendRecord);
                std::string signature = "";
                ecdsa.SignMessage(privateKey, sendRecord.hash, signature);
                sendRecord.signature = signature;
                if(queueAndBroadcastRecord(functions, relayClient, sendRecord)){
                    std::cout << "Sent transfer request. " << std::endl;
                }
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
            CNetworkConfig networkConfig = CNetworkConfig::load();
            long localGenesisBlockId = networkBlockDB.getFirstBlockId();
            CFunctions::block_structure localGenesisBlock = networkBlockDB.getBlock(localGenesisBlockId);
            std::cout << " First block: " << localGenesisBlockId << std::endl;
            std::cout << " Latest block: " << networkBlockDB.getLatestBlockId() << std::endl;
            std::cout << " Genesis match: " << (networkConfig.genesisMatches(localGenesisBlockId, localGenesisBlock.hash) ? "yes" : "no") << std::endl;
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

        } else if ( command.compare("peers") == 0 ){

            CLocalPeerClient::discoverPeers();
            std::vector<CLocalPeerClient::peer_status> peerStatuses = CLocalPeerClient::getPeerStatuses();
            std::cout << " Local peers: " << peerStatuses.size() << std::endl;
            if(peerStatuses.size() == 0){
                std::cout << "  No local peers configured. Start a wallet normally or use --peer http://host:port" << std::endl;
            }
            for(int i = 0; i < peerStatuses.size(); i++){
                CLocalPeerClient::peer_status peer = peerStatuses.at(i);
                if(peer.lastSeenEpoch == 0){
                    CLocalPeerClient::getPeerLatestBlockId(peer.url);
                    peerStatuses = CLocalPeerClient::getPeerStatuses();
                    peer = peerStatuses.at(i);
                }
                std::cout << "  " << peer.url << std::endl;
                std::cout << "    reachable: " << (peer.reachable ? "yes" : "no") << std::endl;
                std::cout << "    genesis match: " << (peer.genesisMatch ? "yes" : "no") << std::endl;
                std::cout << "    protocol: " << peer.protocolVersion << std::endl;
                std::cout << "    latest: " << peer.latestBlockId;
                if(peer.latestBlockHash.length() > 0){
                    std::cout << " hash " << shortKey(peer.latestBlockHash);
                }
                std::cout << std::endl;
                std::cout << "    score: " << peer.score << " successes " << peer.successes << " failures " << peer.failures << std::endl;
                if(peer.lastSuccessEpoch > 0){
                    std::stringstream lastSuccess;
                    lastSuccess << peer.lastSuccessEpoch;
                    std::cout << "    last success: " << formatEpochTimeString(lastSuccess.str()) << std::endl;
                }
                if(peer.firstFailureEpoch > 0){
                    std::stringstream firstFailure;
                    firstFailure << peer.firstFailureEpoch;
                    std::cout << "    failing since: " << formatEpochTimeString(firstFailure.str()) << std::endl;
                    std::cout << "    purge after: 3 days unavailable" << std::endl;
                }
                if(peer.lastError.length() > 0){
                    std::cout << "    last error: " << peer.lastError << std::endl;
                }
            }

        } else if ( command.compare("nextblock") == 0 ){

            CLocalPeerClient::syncNetworkTime();
            printNextBlockSelection(publicKey);

        } else if ( command.compare("memberships") == 0 || command.compare("members") == 0 ){

            printMembershipRecords();

        } else if ( command.compare("carryforwards") == 0 ){

            printCarryForwardRecords();

        } else if ( command.compare("carryforward") == 0 ){

            CBlockDB carryBlockDB;
            long firstBlockId = carryBlockDB.getFirstBlockId();
            long latestConnectedBlockId = carryBlockDB.getConnectedLatestBlockId();
            if(publicKey.length() == 0){
                std::cout << " Wallet public key is not available." << std::endl;
            } else if(firstBlockId < 0 || latestConnectedBlockId < 0){
                std::cout << " No accepted chain available for carry-forward." << std::endl;
            } else if(latestConnectedBlockId - firstBlockId < CFunctions::CARRY_FORWARD_PRUNE_BLOCKS){
                std::cout << " Chain is not old enough for carry-forward yet." << std::endl;
                std::cout << " Required span: " << CFunctions::CARRY_FORWARD_PRUNE_BLOCKS << " blocks" << std::endl;
                std::cout << " Current span: " << (latestConnectedBlockId - firstBlockId) << " blocks" << std::endl;
            } else {
                long checkpointBlock = latestConnectedBlockId - CFunctions::CARRY_FORWARD_PRUNE_BLOCKS;
                long period = checkpointBlock / CFunctions::CARRY_FORWARD_PERIOD_BLOCKS;
                if(acceptedCarryForwardExists(publicKey, period)){
                    std::cout << " Carry-forward already accepted for this wallet and period." << std::endl;
                    std::cout << " Period: " << period << std::endl;
                } else {
                    double checkpointBalance = accountBalanceAtBlock(publicKey, checkpointBlock);
                    if(checkpointBalance <= 0){
                        std::cout << " Carry-forward not queued; checkpoint balance is not positive." << std::endl;
                        std::cout << " Checkpoint block: " << checkpointBlock << std::endl;
                        std::cout << " Balance at checkpoint: " << checkpointBalance << " sfr" << std::endl;
                    } else {
                        CFunctions::record_structure carryForwardRecord;
                        carryForwardRecord.network = "main";
                        CNetworkTime netTime;
                        std::stringstream ss;
                        ss << netTime.getEpoch();
                        carryForwardRecord.time = ss.str();
                        carryForwardRecord.transaction_type = CFunctions::CARRY_FORWARD;
                        carryForwardRecord.amount = checkpointBalance;
                        carryForwardRecord.fee = 0.0;
                        carryForwardRecord.sender_public_key = publicKey;
                        carryForwardRecord.recipient_public_key = publicKey;
                        carryForwardRecord.name = "carry-forward";
                        carryForwardRecord.value = carryForwardValue(checkpointBlock, period);
                        carryForwardRecord.hash = functions.getRecordHash(carryForwardRecord);
                        std::string signature = "";
                        ecdsa.SignMessage(privateKey, carryForwardRecord.hash, signature);
                        carryForwardRecord.signature = signature;

                        if(queueAndBroadcastRecord(functions, relayClient, carryForwardRecord)){
                            std::cout << " Carry-forward queued and broadcast." << std::endl;
                            std::cout << " Account: " << publicKey << std::endl;
                            std::cout << " Checkpoint block: " << checkpointBlock << std::endl;
                            std::cout << " Period: " << period << std::endl;
                            std::cout << " Snapshot balance: " << checkpointBalance << " sfr" << std::endl;
                            std::cout << " Reward after accepted: " << CFunctions::CARRY_FORWARD_REWARD << " sfr" << std::endl;
                        }
                    }
                }
            }

        } else if ( command.compare("gui") == 0 || command.compare("launchgui") == 0 ){

            std::string message;
            launchGuiInterface(message);
            std::cout << " " << message << std::endl;
 
        } else if ( command.find("quit") != std::string::npos ){
                running = false;
            
            } else if ( command.find("advanced") != std::string::npos ){
                printAdvancedCommands();
            } else if ( command.find("tests") != std::string::npos ){
                CECDSACrypto ecdsa;
                ecdsa.runTests();



        } else if ( command.compare("chainid") == 0){
           printChainIdentity();

        } else if ( command.compare("chain") == 0){
           printChainDiagnostics();

        } else if ( command.compare("repairchain") == 0){
            CBlockDB blockDB;
            long before = blockDB.getConnectedLatestBlockId();
            long after = blockDB.rebuildBestChainIndex();
            std::cout << " Chain repair:" << std::endl;
            std::cout << "  connected latest before: " << before << std::endl;
            std::cout << "  connected latest after: " << after << std::endl;
	
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
            
        } else if(command.compare("syncfull") == 0){
            std::cout << " Full syncing local peers... " << std::endl;
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
                CLocalPeerClient::push_result pushResult = CLocalPeerClient::pushFullChainToPeerDetailed(localPeers.at(i));
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
        } else if ( command.compare("votes") == 0){

            printVoteRecords();

        } else if ( command.compare("vote") == 0){
            std::cout << " Vote name: " << std::endl;
            std::string voteName;
            std::cin >> voteName;
            std::cout << " Vote value: " << std::endl;
            std::string voteValue;
            std::cin >> voteValue;

            if(voteName.length() == 0 || voteValue.length() == 0){
                std::cout << " Vote not sent. Name and value are required." << std::endl;
            } else {
                CFunctions::record_structure voteRecord;
                voteRecord.network = "main";
                CNetworkTime netTime;
                std::stringstream ss;
                ss << netTime.getEpoch();
                std::string ts = ss.str();
                voteRecord.time = ts;
                voteRecord.transaction_type = CFunctions::VOTE;
                voteRecord.name = voteName;
                voteRecord.value = voteValue;
                voteRecord.amount = 0.0;
                voteRecord.fee = 0.0;
                voteRecord.sender_public_key = publicKey;
                voteRecord.recipient_public_key = "";

                voteRecord.hash = functions.getRecordHash(voteRecord);
                std::string signature = "";
                ecdsa.SignMessage(privateKey, voteRecord.hash, signature);
                voteRecord.signature = signature;

                if(queueAndBroadcastRecord(functions, relayClient, voteRecord)){
                    std::cout << "Vote queued and broadcast. It will appear in votes after a block includes it." << std::endl;
                    std::cout << " Name: " << voteName << std::endl;
                    std::cout << " Value: " << voteValue << std::endl;
                }
            }

	} else {
		printCommands();	
	}
    }
}
