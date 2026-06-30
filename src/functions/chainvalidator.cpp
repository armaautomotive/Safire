// Copyright (c) 2026 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/chainvalidator.h"

#include "blockdb.h"
#include "ecdsacrypto.h"
#include "functions/ledgerstate.h"
#include "functions/selector.h"
#include "networkconfig.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

namespace {

std::string normalizedPublicName(const std::string& name)
{
    if (name.length() == 0) {
        return "";
    }
    if (name.length() < 3 || name.length() > 32) {
        return "#invalid";
    }

    std::string normalized;
    for (int i = 0; i < name.length(); ++i) {
        unsigned char ch = static_cast<unsigned char>(name.at(i));
        if (std::isalnum(ch)) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        } else if (ch == '-' || ch == '_') {
            normalized.push_back(static_cast<char>(ch));
        } else {
            return "#invalid";
        }
    }
    return normalized;
}

bool recordRequiresSenderSignature(const CFunctions::record_structure& record)
{
    return record.transaction_type == CFunctions::JOIN_NETWORK ||
           record.transaction_type == CFunctions::ISSUE_CURRENCY ||
           record.transaction_type == CFunctions::TRANSFER_CURRENCY ||
           record.transaction_type == CFunctions::CARRY_FORWARD ||
           record.transaction_type == CFunctions::VOTE ||
           record.transaction_type == CFunctions::HEART_BEAT ||
           record.transaction_type == CFunctions::UPDATE_NAME;
}

bool recordExistsInChain(CBlockDB& blockDB, const std::string& recordHash, long stopBlock)
{
    if (recordHash.length() == 0) {
        return false;
    }

    long firstBlockId = blockDB.getFirstBlockId();
    if (firstBlockId < 0 || stopBlock < firstBlockId) {
        return false;
    }

    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    int guard = 0;
    while (block.number > 0 && guard < 100000) {
        if (block.number > stopBlock) {
            break;
        }
        for (int i = 0; i < block.records.size(); ++i) {
            if (block.records.at(i).hash.compare(recordHash) == 0) {
                return true;
            }
        }
        if (block.number == stopBlock) {
            break;
        }
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
        if (nextBlock.number <= 0 || nextBlock.number == block.number) {
            break;
        }
        block = nextBlock;
        ++guard;
    }
    return false;
}

bool nameTakenByAnother(const CLedgerState::state& state, const std::string& name, const std::string& owner)
{
    std::string normalized = normalizedPublicName(name);
    if (normalized.length() == 0) {
        return false;
    }
    if (normalized == "#invalid") {
        return true;
    }

    for (std::map<std::string, std::string>::const_iterator it = state.names.begin(); it != state.names.end(); ++it) {
        if (normalizedPublicName(it->second).compare(normalized) == 0 && it->first.compare(owner) != 0) {
            return true;
        }
    }
    return false;
}

bool isJoinedMember(const CLedgerState::state& state, const std::string& publicKey)
{
    for (int i = 0; i < state.members.size(); ++i) {
        if (state.members.at(i).public_key.compare(publicKey) == 0) {
            return true;
        }
    }
    return false;
}

struct BranchMember {
    std::string public_key;
    std::string name;
    long join_block;
    long last_heartbeat_block;
};

int branchMemberIndexByKey(const std::vector<BranchMember>& members, const std::string& publicKey)
{
    for (int i = 0; i < members.size(); ++i) {
        if (members.at(i).public_key.compare(publicKey) == 0) {
            return i;
        }
    }
    return -1;
}

bool branchNameTakenByAnother(const std::map<std::string, std::string>& nameOwners, const std::string& name, const std::string& owner)
{
    std::string normalized = normalizedPublicName(name);
    if (normalized.length() == 0) {
        return false;
    }
    if (normalized == "#invalid") {
        return true;
    }

    std::map<std::string, std::string>::const_iterator it = nameOwners.find(normalized);
    return it != nameOwners.end() && it->second.compare(owner) != 0;
}

bool claimBranchName(
    std::vector<BranchMember>& members,
    std::map<std::string, std::string>& nameOwners,
    const std::string& publicKey,
    const std::string& name)
{
    std::string normalized = normalizedPublicName(name);
    if (normalized.length() == 0 || normalized == "#invalid") {
        return false;
    }

    int memberIndex = branchMemberIndexByKey(members, publicKey);
    if (memberIndex < 0) {
        return false;
    }

    if (branchNameTakenByAnother(nameOwners, name, publicKey)) {
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

std::vector<std::string> activeMemberKeysForBlock(const std::vector<BranchMember>& members, bool chainHasHeartbeatRecords, long blockNumber)
{
    std::vector<std::string> active;
    long heartbeatCutoff = blockNumber - CFunctions::HEARTBEAT_VALID_BLOCKS;
    for (int i = 0; i < members.size(); ++i) {
        if (chainHasHeartbeatRecords == false || members.at(i).last_heartbeat_block >= heartbeatCutoff) {
            active.push_back(members.at(i).public_key);
        }
    }
    return active;
}

std::vector<std::string> activeMemberKeysForBlock(const std::vector<CLedgerState::member_state>& members, bool chainHasHeartbeatRecords, long blockNumber)
{
    std::vector<std::string> active;
    long heartbeatCutoff = blockNumber - CFunctions::HEARTBEAT_VALID_BLOCKS;
    for (int i = 0; i < members.size(); ++i) {
        if (chainHasHeartbeatRecords == false || members.at(i).last_heartbeat_block >= heartbeatCutoff) {
            active.push_back(members.at(i).public_key);
        }
    }
    return active;
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

} // namespace

bool CChainValidator::validateBlockForStorage(CBlockDB& blockDB, const CFunctions::block_structure& block, std::string& reason)
{
    CFunctions functions;
    CECDSACrypto ecdsa;
    reason = "";

    if (block.number <= 0) {
        reason = "block number is invalid";
        return false;
    }
    if (block.hash.length() == 0) {
        reason = "block hash is missing";
        return false;
    }
    if (block.records.size() > CFunctions::MAX_BLOCK_RECORDS) {
        reason = "too many records";
        return false;
    }
    if (block.records_merkle_root.length() > 0) {
        std::string expectedRecordsMerkleRoot = functions.getRecordsMerkleRoot(block.records);
        if (expectedRecordsMerkleRoot.compare(block.records_merkle_root) != 0) {
            reason = "records merkle root mismatch";
            return false;
        }
    }

    std::string expectedBlockHash = functions.getBlockHash(block);
    if (expectedBlockHash.compare(block.hash) != 0) {
        reason = "block hash mismatch";
        return false;
    }
    if (block.creator_key.length() == 0) {
        reason = "block creator is missing";
        return false;
    }
    if (block.signature.length() == 0 ||
        ecdsa.VerifyMessageCompressed(block.hash, block.signature, block.creator_key) != 1) {
        reason = "block signature is invalid";
        return false;
    }

    CFunctions::block_structure parent;
    bool isGenesis = block.previous_block_id <= 0;
    if (isGenesis) {
        CNetworkConfig config = CNetworkConfig::load();
        if (config.genesisMatches(block.number, block.hash) == false) {
            reason = "genesis block does not match safire.conf";
            return false;
        }
    } else {
        if (block.previous_block_hash.length() == 0) {
            reason = "previous block hash is missing";
            return false;
        }
        parent = blockDB.getBlockByHash(block.previous_block_hash);
        if (parent.number <= 0) {
            reason = "parent block is missing";
            return false;
        }
        if (parent.number != block.previous_block_id) {
            reason = "previous block id does not match parent hash";
            return false;
        }
        if (block.previous_block_id >= block.number) {
            reason = "previous block id is not before block number";
            return false;
        }

        CFunctions::block_structure canonicalParent = blockDB.getBlock(parent.number);
        bool parentIsCanonical = canonicalParent.number == parent.number && canonicalParent.hash.compare(parent.hash) == 0;
        if (parentIsCanonical) {
            CLedgerState::state parentState = CLedgerState::build(blockDB, "", parent.number);
            std::vector<std::string> activeMemberKeys = activeMemberKeysForBlock(parentState.members, parentState.chain_has_heartbeat_records, block.number);
            if (activeMemberKeys.empty()) {
                reason = "no active members can create blocks";
                return false;
            }
            std::string selectedCreator = CSelector::getSelectedUserForBlock(block.number, parent.hash, activeMemberKeys);
            if (selectedCreator.compare(block.creator_key) != 0) {
                reason = "block creator is not selected for this slot";
                return false;
            }
        }
    }

    CFunctions::block_structure canonicalParent = isGenesis ? CFunctions::block_structure() : blockDB.getBlock(parent.number);
    bool enforceStateRules = isGenesis || (canonicalParent.number == parent.number && canonicalParent.hash.compare(parent.hash) == 0);
    CLedgerState::state parentState = (!isGenesis && enforceStateRules) ? CLedgerState::build(blockDB, "", parent.number) : CLedgerState::state();
    std::map<std::string, double> temporaryBalances = parentState.balances;
    std::set<std::string> blockRecordHashes;

    for (int i = 0; i < block.records.size(); ++i) {
        CFunctions::record_structure record = block.records.at(i);
        if (functions.isRecordSizeValid(record) == false) {
            reason = "record is oversized";
            return false;
        }

        std::string expectedRecordHash = functions.getRecordHash(record);
        if (record.hash.length() == 0 || expectedRecordHash.compare(record.hash) != 0) {
            reason = "record hash mismatch";
            return false;
        }
        if (blockRecordHashes.find(record.hash) != blockRecordHashes.end()) {
            reason = "duplicate record hash in block";
            return false;
        }
        blockRecordHashes.insert(record.hash);
        if (!isGenesis && recordExistsInChain(blockDB, record.hash, parent.number)) {
            reason = "duplicate record hash already exists in chain";
            return false;
        }

        if (recordRequiresSenderSignature(record)) {
            if (record.sender_public_key.length() == 0) {
                reason = "record sender key is missing";
                return false;
            }
            if (record.signature.length() == 0 ||
                ecdsa.VerifyMessageCompressed(record.hash, record.signature, record.sender_public_key) != 1) {
                reason = "record signature is invalid";
                return false;
            }
        }

        if (record.transaction_type == CFunctions::ISSUE_CURRENCY) {
            if (record.amount < 0.0 || record.amount > 1.0) {
                reason = "issue amount is invalid";
                return false;
            }
            if (record.sender_public_key.compare(block.creator_key) != 0 ||
                record.recipient_public_key.compare(block.creator_key) != 0) {
                reason = "issue reward must pay the block creator";
                return false;
            }
            temporaryBalances[record.recipient_public_key] += record.amount;
        } else if (record.transaction_type == CFunctions::TRANSFER_CURRENCY) {
            if (record.amount <= 0.0 || record.fee < 0.0) {
                reason = "transfer amount or fee is invalid";
                return false;
            }
            double available = temporaryBalances[record.sender_public_key];
            double debit = record.sender_public_key.compare(record.recipient_public_key) == 0 ? record.fee : record.amount + record.fee;
            if (enforceStateRules && available + 0.000001 < debit) {
                reason = "transfer overspends sender balance";
                return false;
            }
            temporaryBalances[record.recipient_public_key] += record.amount;
            temporaryBalances[record.sender_public_key] -= record.amount + record.fee;
            temporaryBalances[block.creator_key] += record.fee;
        } else if (record.transaction_type == CFunctions::VOTE) {
            if (record.fee < 0.0 || (enforceStateRules && temporaryBalances[record.sender_public_key] + 0.000001 < record.fee)) {
                reason = "vote fee overspends sender balance";
                return false;
            }
            temporaryBalances[record.sender_public_key] -= record.fee;
            temporaryBalances[block.creator_key] += record.fee;
        } else if (record.transaction_type == CFunctions::JOIN_NETWORK) {
            if (enforceStateRules && !isGenesis && isJoinedMember(parentState, record.sender_public_key)) {
                reason = "member already joined";
                return false;
            }
            if (enforceStateRules && record.name.length() > 0 && nameTakenByAnother(parentState, record.name, record.sender_public_key)) {
                reason = "public name is unavailable";
                return false;
            }
        } else if (record.transaction_type == CFunctions::UPDATE_NAME) {
            if (enforceStateRules && !isJoinedMember(parentState, record.sender_public_key)) {
                reason = "name update sender is not a member";
                return false;
            }
            if (record.name.length() == 0 || (enforceStateRules && nameTakenByAnother(parentState, record.name, record.sender_public_key))) {
                reason = "public name is unavailable";
                return false;
            }
        } else if (record.transaction_type == CFunctions::HEART_BEAT) {
            if (enforceStateRules && !isGenesis && !isJoinedMember(parentState, record.sender_public_key)) {
                reason = "heartbeat sender is not a member";
                return false;
            }
        } else if (record.transaction_type == CFunctions::CARRY_FORWARD) {
            long checkpointBalanceBlock = ::atol(record.value.c_str());
            (void)checkpointBalanceBlock;
            if (record.amount < 0.0) {
                reason = "carry-forward amount is invalid";
                return false;
            }
        }
    }

    return true;
}

bool CChainValidator::validateConnectedChain(const std::vector<CFunctions::block_structure>& chain, std::string& reason)
{
    CFunctions functions;
    CECDSACrypto ecdsa;
    reason = "";

    if (chain.size() == 0) {
        reason = "candidate chain is empty";
        return false;
    }

    CNetworkConfig config = CNetworkConfig::load();
    std::map<std::string, double> balances;
    std::vector<BranchMember> members;
    std::map<std::string, std::string> nameOwners;
    std::set<std::string> acceptedRecordHashes;
    std::set<std::string> acceptedCarryForwardKeys;
    bool chainHasHeartbeatRecords = false;

    for (int b = 0; b < chain.size(); ++b) {
        CFunctions::block_structure block = chain.at(b);
        bool isGenesis = b == 0;

        if (block.number <= 0) {
            reason = "block number is invalid";
            return false;
        }
        if (block.hash.length() == 0) {
            reason = "block hash is missing";
            return false;
        }
        if (block.records.size() > CFunctions::MAX_BLOCK_RECORDS) {
            reason = "too many records";
            return false;
        }
        if (block.records_merkle_root.length() > 0) {
            std::string expectedRecordsMerkleRoot = functions.getRecordsMerkleRoot(block.records);
            if (expectedRecordsMerkleRoot.compare(block.records_merkle_root) != 0) {
                reason = "records merkle root mismatch";
                return false;
            }
        }
        std::string expectedBlockHash = functions.getBlockHash(block);
        if (expectedBlockHash.compare(block.hash) != 0) {
            reason = "block hash mismatch";
            return false;
        }
        if (block.creator_key.length() == 0) {
            reason = "block creator is missing";
            return false;
        }
        if (block.signature.length() == 0 ||
            ecdsa.VerifyMessageCompressed(block.hash, block.signature, block.creator_key) != 1) {
            reason = "block signature is invalid";
            return false;
        }

        if (isGenesis) {
            if (block.previous_block_id > 0 || block.previous_block_hash.length() > 0) {
                reason = "genesis block has a parent";
                return false;
            }
            if (config.genesisMatches(block.number, block.hash) == false) {
                reason = "genesis block does not match safire.conf";
                return false;
            }
        } else {
            CFunctions::block_structure parent = chain.at(b - 1);
            if (block.previous_block_id != parent.number) {
                reason = "previous block id does not match parent";
                return false;
            }
            if (block.previous_block_id >= block.number) {
                reason = "previous block id is not before block number";
                return false;
            }
            if (block.previous_block_hash.compare(parent.hash) != 0) {
                reason = "previous block hash does not match parent";
                return false;
            }

            std::vector<std::string> activeMemberKeys = activeMemberKeysForBlock(members, chainHasHeartbeatRecords, block.number);
            if (activeMemberKeys.empty()) {
                reason = "no active members can create blocks";
                return false;
            }
            std::string selectedCreator = CSelector::getSelectedUserForBlock(block.number, parent.hash, activeMemberKeys);
            if (selectedCreator.compare(block.creator_key) != 0) {
                reason = "block creator is not selected for this slot";
                return false;
            }
        }

        std::set<std::string> blockRecordHashes;
        for (int r = 0; r < block.records.size(); ++r) {
            CFunctions::record_structure record = block.records.at(r);
            if (functions.isRecordSizeValid(record) == false) {
                reason = "record is oversized";
                return false;
            }

            std::string expectedRecordHash = functions.getRecordHash(record);
            if (record.hash.length() == 0 || expectedRecordHash.compare(record.hash) != 0) {
                reason = "record hash mismatch";
                return false;
            }
            if (blockRecordHashes.find(record.hash) != blockRecordHashes.end()) {
                reason = "duplicate record hash in block";
                return false;
            }
            blockRecordHashes.insert(record.hash);
            if (acceptedRecordHashes.find(record.hash) != acceptedRecordHashes.end()) {
                reason = "duplicate record hash already exists in chain";
                return false;
            }
            acceptedRecordHashes.insert(record.hash);

            if (recordRequiresSenderSignature(record)) {
                if (record.sender_public_key.length() == 0) {
                    reason = "record sender key is missing";
                    return false;
                }
                if (record.signature.length() == 0 ||
                    ecdsa.VerifyMessageCompressed(record.hash, record.signature, record.sender_public_key) != 1) {
                    reason = "record signature is invalid";
                    return false;
                }
            }

            if (record.transaction_type == CFunctions::ISSUE_CURRENCY) {
                if (record.amount < 0.0 || record.amount > 1.0) {
                    reason = "issue amount is invalid";
                    return false;
                }
                if (record.sender_public_key.compare(block.creator_key) != 0 ||
                    record.recipient_public_key.compare(block.creator_key) != 0) {
                    reason = "issue reward must pay the block creator";
                    return false;
                }
                balances[record.recipient_public_key] += record.amount;
            } else if (record.transaction_type == CFunctions::TRANSFER_CURRENCY) {
                if (record.amount <= 0.0 || record.fee < 0.0) {
                    reason = "transfer amount or fee is invalid";
                    return false;
                }
                double debit = record.sender_public_key.compare(record.recipient_public_key) == 0 ? record.fee : record.amount + record.fee;
                if (balances[record.sender_public_key] + 0.000001 < debit) {
                    reason = "transfer overspends sender balance";
                    return false;
                }
                balances[record.recipient_public_key] += record.amount;
                balances[record.sender_public_key] -= record.amount + record.fee;
                balances[block.creator_key] += record.fee;
            } else if (record.transaction_type == CFunctions::VOTE) {
                if (record.fee < 0.0 || balances[record.sender_public_key] + 0.000001 < record.fee) {
                    reason = "vote fee overspends sender balance";
                    return false;
                }
                balances[record.sender_public_key] -= record.fee;
                balances[block.creator_key] += record.fee;
            } else if (record.transaction_type == CFunctions::JOIN_NETWORK) {
                if (!isGenesis && branchMemberIndexByKey(members, record.sender_public_key) >= 0) {
                    reason = "member already joined";
                    return false;
                }
                if (record.name.length() > 0 && branchNameTakenByAnother(nameOwners, record.name, record.sender_public_key)) {
                    reason = "public name is unavailable";
                    return false;
                }
                if (branchMemberIndexByKey(members, record.sender_public_key) < 0) {
                    BranchMember member;
                    member.public_key = record.sender_public_key;
                    member.name = "";
                    member.join_block = block.number;
                    member.last_heartbeat_block = -1;
                    members.push_back(member);
                }
                if (record.name.length() > 0 && !claimBranchName(members, nameOwners, record.sender_public_key, record.name)) {
                    reason = "public name is unavailable";
                    return false;
                }
            } else if (record.transaction_type == CFunctions::UPDATE_NAME) {
                if (branchMemberIndexByKey(members, record.sender_public_key) < 0) {
                    reason = "name update sender is not a member";
                    return false;
                }
                if (record.name.length() == 0 || !claimBranchName(members, nameOwners, record.sender_public_key, record.name)) {
                    reason = "public name is unavailable";
                    return false;
                }
            } else if (record.transaction_type == CFunctions::HEART_BEAT) {
                int memberIndex = branchMemberIndexByKey(members, record.sender_public_key);
                if (!isGenesis && memberIndex < 0) {
                    reason = "heartbeat sender is not a member";
                    return false;
                }
                if (memberIndex >= 0) {
                    members[memberIndex].last_heartbeat_block = block.number;
                    chainHasHeartbeatRecords = true;
                }
            } else if (record.transaction_type == CFunctions::CARRY_FORWARD) {
                long checkpointBlock = parseCarryForwardValueLong(record.value, "checkpoint");
                long period = parseCarryForwardValueLong(record.value, "period");
                if (record.amount < 0.0 || checkpointBlock < 0 || period < 0 || checkpointBlock >= block.number) {
                    reason = "carry-forward amount is invalid";
                    return false;
                }
                std::string key = carryForwardKey(record);
                if (key.length() > 0 && acceptedCarryForwardKeys.find(key) == acceptedCarryForwardKeys.end()) {
                    acceptedCarryForwardKeys.insert(key);
                    balances[record.sender_public_key] += CFunctions::CARRY_FORWARD_REWARD;
                }
            } else if (record.transaction_type == CFunctions::PERIOD_SUMMARY) {
                reason = "period summary records are not accepted";
                return false;
            } else {
                reason = "unknown record type";
                return false;
            }
        }
    }

    return true;
}
