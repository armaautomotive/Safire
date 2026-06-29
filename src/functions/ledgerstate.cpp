// Copyright (c) 2026 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/ledgerstate.h"

#include "blockdb.h"
#include "functions/selector.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <sstream>

namespace {

std::string normalizedPublicName(const std::string& name)
{
    if (name.length() < 3 || name.length() > 32) {
        return "";
    }

    std::string normalized;
    for (int i = 0; i < name.length(); ++i) {
        unsigned char ch = static_cast<unsigned char>(name.at(i));
        if (std::isalnum(ch)) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        } else if (ch == '-' || ch == '_') {
            normalized.push_back(static_cast<char>(ch));
        } else {
            return "";
        }
    }
    return normalized;
}

void addBalanceDelta(std::map<std::string, double>& balances, const std::string& publicKey, double amount)
{
    if (publicKey.length() == 0) {
        return;
    }
    balances[publicKey] += amount;
}

long parseCarryForwardValueLong(const std::string& value, const std::string& key)
{
    std::string prefix = key + "=";
    std::size_t start = value.find(prefix);
    if (start == std::string::npos) {
        return -1;
    }
    start += prefix.length();
    std::size_t end = value.find(";", start);
    std::string section = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (section.length() == 0) {
        return -1;
    }
    return ::atol(section.c_str());
}

std::string carryForwardKey(const CFunctions::record_structure& record)
{
    long period = parseCarryForwardValueLong(record.value, "period");
    if (record.recipient_public_key.length() == 0 || period < 0) {
        return "";
    }

    std::stringstream ss;
    ss << record.recipient_public_key << ":" << period;
    return ss.str();
}

bool isBasicCarryForwardRecord(const CFunctions::record_structure& record, long blockNumber)
{
    if (record.transaction_type != CFunctions::CARRY_FORWARD) {
        return false;
    }
    if (record.sender_public_key.length() == 0 || record.recipient_public_key.length() == 0) {
        return false;
    }
    long checkpoint = parseCarryForwardValueLong(record.value, "checkpoint");
    long period = parseCarryForwardValueLong(record.value, "period");
    if (checkpoint < 0 || period < 0 || checkpoint >= blockNumber) {
        return false;
    }
    return record.amount >= 0;
}

int memberIndexByKey(const std::vector<CLedgerState::member_state>& members, const std::string& publicKey)
{
    for (int i = 0; i < members.size(); ++i) {
        if (members.at(i).public_key.compare(publicKey) == 0) {
            return i;
        }
    }
    return -1;
}

bool claimPublicName(
    std::vector<CLedgerState::member_state>& members,
    std::map<std::string, std::string>& nameOwners,
    const std::string& publicKey,
    const std::string& name)
{
    std::string normalized = normalizedPublicName(name);
    if (normalized.length() == 0) {
        return false;
    }

    int memberIndex = memberIndexByKey(members, publicKey);
    if (memberIndex < 0) {
        return false;
    }

    std::map<std::string, std::string>::iterator owner = nameOwners.find(normalized);
    if (owner != nameOwners.end() && owner->second.compare(publicKey) != 0) {
        return false;
    }

    std::string previousName = normalizedPublicName(members.at(memberIndex).name);
    if (previousName.length() > 0) {
        std::map<std::string, std::string>::iterator previousOwner = nameOwners.find(previousName);
        if (previousOwner != nameOwners.end() && previousOwner->second.compare(publicKey) == 0) {
            nameOwners.erase(previousOwner);
        }
    }

    members[memberIndex].name = name;
    nameOwners[normalized] = publicKey;
    return true;
}

void applyRecordToBalances(
    const CFunctions::block_structure& block,
    const CFunctions::record_structure& record,
    std::map<std::string, double>& balances,
    double& issuedSupply)
{
    if (record.transaction_type == CFunctions::ISSUE_CURRENCY) {
        double amount = record.amount;
        if (amount > 1.0 || amount < 0.0) {
            amount = 1.0;
        }
        addBalanceDelta(balances, record.recipient_public_key, amount);
        issuedSupply += amount;
    } else if (record.transaction_type == CFunctions::TRANSFER_CURRENCY) {
        addBalanceDelta(balances, record.recipient_public_key, record.amount);
        addBalanceDelta(balances, record.sender_public_key, -record.amount - record.fee);
        addBalanceDelta(balances, block.creator_key, record.fee);
    } else if (record.transaction_type == CFunctions::VOTE) {
        addBalanceDelta(balances, record.sender_public_key, -record.fee);
        addBalanceDelta(balances, block.creator_key, record.fee);
    }
}

std::map<std::string, double> balancesThroughBlock(CBlockDB& blockDB, long checkpointBlock)
{
    std::map<std::string, double> balances;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if (firstBlockId < 0 || latestBlockId < 0 || checkpointBlock < firstBlockId) {
        return balances;
    }

    double ignoredSupply = 0.0;
    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    std::set<std::string> acceptedRecordHashes;
    int guard = 0;
    while (block.number > 0 && guard < 100000) {
        if (block.number > checkpointBlock) {
            break;
        }

        for (int i = 0; i < block.records.size(); ++i) {
            CFunctions::record_structure record = block.records.at(i);
            if (record.hash.length() > 0 && acceptedRecordHashes.find(record.hash) != acceptedRecordHashes.end()) {
                continue;
            }
            if (record.hash.length() > 0) {
                acceptedRecordHashes.insert(record.hash);
            }
            applyRecordToBalances(block, record, balances, ignoredSupply);
        }

        if (block.number == latestBlockId) {
            break;
        }
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
        if (nextBlock.number <= 0 || nextBlock.number == block.number) {
            break;
        }
        block = nextBlock;
        ++guard;
    }
    return balances;
}

bool carryForwardSnapshotMatches(CBlockDB& blockDB, const CFunctions::record_structure& record)
{
    long checkpoint = parseCarryForwardValueLong(record.value, "checkpoint");
    if (checkpoint < 0) {
        return false;
    }

    std::map<std::string, double> balances = balancesThroughBlock(blockDB, checkpoint);
    double expectedBalance = balances[record.recipient_public_key];
    return std::fabs(expectedBalance - record.amount) < 0.000001;
}

} // namespace

CLedgerState::state CLedgerState::build(CBlockDB& blockDB, const std::string& walletPublicKey)
{
    CLedgerState::state result;
    result.issued_supply = 0.0;
    result.ledger_balance_total = 0.0;
    result.wallet_balance = 0.0;
    result.joined = false;
    result.active_heartbeat = false;
    result.heartbeat_renewal_due = false;
    result.chain_has_heartbeat_records = false;
    result.last_heartbeat_block = -1;
    result.first_block_id = blockDB.getFirstBlockId();
    result.latest_block_id = blockDB.getLatestBlockId();
    result.connected_block_count = 0;

    if (result.first_block_id < 0 || result.latest_block_id < 0) {
        return result;
    }

    std::map<std::string, std::string> nameOwners;
    std::map<std::string, long> latestHeartbeatBlockByUser;
    std::set<std::string> acceptedRecordHashes;
    std::set<std::string> acceptedCarryForwardKeys;
    CSelector selector;
    long currentTimeBlock = selector.getCurrentTimeBlock();
    long heartbeatCutoff = currentTimeBlock - CFunctions::HEARTBEAT_VALID_BLOCKS;
    long heartbeatRenewCutoff = currentTimeBlock - CFunctions::HEARTBEAT_RENEW_BLOCKS;

    CFunctions::block_structure block = blockDB.getBlock(result.first_block_id);
    int guard = 0;
    while (block.number > 0 && guard < 100000) {
        result.latest_block = block;
        result.connected_block_count++;

        for (int i = 0; i < block.records.size(); ++i) {
            CFunctions::record_structure record = block.records.at(i);
            if (record.hash.length() > 0 && acceptedRecordHashes.find(record.hash) != acceptedRecordHashes.end()) {
                continue;
            }
            if (record.hash.length() > 0) {
                acceptedRecordHashes.insert(record.hash);
            }

            if (record.transaction_type == CFunctions::JOIN_NETWORK && record.sender_public_key.length() > 0) {
                if (memberIndexByKey(result.members, record.sender_public_key) < 0) {
                    CLedgerState::member_state member;
                    member.public_key = record.sender_public_key;
                    member.name = "";
                    member.join_block = block.number;
                    member.last_heartbeat_block = -1;
                    member.active = false;
                    result.members.push_back(member);
                    if (record.name.length() > 0) {
                        claimPublicName(result.members, nameOwners, record.sender_public_key, record.name);
                    }
                }
                if (record.sender_public_key.compare(walletPublicKey) == 0) {
                    result.joined = true;
                }
            } else if (record.transaction_type == CFunctions::UPDATE_NAME &&
                       record.sender_public_key.length() > 0 &&
                       record.name.length() > 0) {
                claimPublicName(result.members, nameOwners, record.sender_public_key, record.name);
            } else if (record.transaction_type == CFunctions::HEART_BEAT &&
                       record.sender_public_key.length() > 0) {
                result.chain_has_heartbeat_records = true;
                latestHeartbeatBlockByUser[record.sender_public_key] = block.number;
                if (record.sender_public_key.compare(walletPublicKey) == 0) {
                    result.last_heartbeat_block = block.number;
                }
            } else if (isBasicCarryForwardRecord(record, block.number) && carryForwardSnapshotMatches(blockDB, record)) {
                std::string key = carryForwardKey(record);
                if (key.length() > 0 && acceptedCarryForwardKeys.find(key) == acceptedCarryForwardKeys.end()) {
                    acceptedCarryForwardKeys.insert(key);
                    addBalanceDelta(result.balances, record.sender_public_key, CFunctions::CARRY_FORWARD_REWARD);
                    result.issued_supply += CFunctions::CARRY_FORWARD_REWARD;
                }
            }

            applyRecordToBalances(block, record, result.balances, result.issued_supply);
        }

        if (block.number == result.latest_block_id) {
            break;
        }
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
        if (nextBlock.number <= 0 || nextBlock.number == block.number) {
            break;
        }
        block = nextBlock;
        ++guard;
    }

    for (int i = 0; i < result.members.size(); ++i) {
        std::string memberKey = result.members.at(i).public_key;
        result.members[i].last_heartbeat_block = latestHeartbeatBlockByUser.find(memberKey) == latestHeartbeatBlockByUser.end() ?
            -1 : latestHeartbeatBlockByUser[memberKey];
        if (result.chain_has_heartbeat_records == false ||
            result.members.at(i).last_heartbeat_block >= heartbeatCutoff) {
            result.members[i].active = true;
            result.active_member_keys.push_back(memberKey);
        }
        if (result.members.at(i).name.length() > 0) {
            result.names[memberKey] = result.members.at(i).name;
        }
    }

    if (result.joined) {
        if (result.chain_has_heartbeat_records == false) {
            result.active_heartbeat = true;
            result.heartbeat_renewal_due = true;
        } else if (result.last_heartbeat_block >= heartbeatCutoff) {
            result.active_heartbeat = true;
            result.heartbeat_renewal_due = result.last_heartbeat_block < heartbeatRenewCutoff;
        } else {
            result.active_heartbeat = false;
            result.heartbeat_renewal_due = true;
        }
    }

    for (std::map<std::string, double>::const_iterator it = result.balances.begin(); it != result.balances.end(); ++it) {
        result.ledger_balance_total += it->second;
    }
    if (walletPublicKey.length() > 0) {
        result.wallet_balance = result.balances[walletPublicKey];
    }

    return result;
}

double CLedgerState::balanceAtBlock(CBlockDB& blockDB, const std::string& publicKey, long checkpointBlock)
{
    if (publicKey.length() == 0) {
        return 0.0;
    }
    std::map<std::string, double> balances = balancesThroughBlock(blockDB, checkpointBlock);
    return balances[publicKey];
}
