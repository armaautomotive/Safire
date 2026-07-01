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
#include <vector>

namespace {

std::map<std::string, CLedgerState::state> validationStateByBlockHash;

bool carryForwardSnapshotMatches(CBlockDB& blockDB, const CFunctions::record_structure& record);
std::string carryForwardKey(const CFunctions::record_structure& record);
bool isBasicCarryForwardRecord(const CFunctions::record_structure& record, long blockNumber);

int memberIndexByKey(std::vector<CLedgerState::member_state>& members, const std::string& publicKey)
{
    for (int i = 0; i < members.size(); ++i) {
        if (members.at(i).public_key.compare(publicKey) == 0) {
            return i;
        }
    }
    return -1;
}

bool cachedLedgerStateForBlock(const CFunctions::block_structure& block, CLedgerState::state& state)
{
    std::map<std::string, CLedgerState::state>::iterator it = validationStateByBlockHash.find(block.hash);
    if (it == validationStateByBlockHash.end()) {
        return false;
    }
    if (it->second.latest_block_id != block.number ||
        it->second.latest_block.hash.compare(block.hash) != 0) {
        return false;
    }
    state = it->second;
    return true;
}

void cacheLedgerStateForBlock(const CFunctions::block_structure& block, const CLedgerState::state& state)
{
    if (block.hash.length() == 0 || block.number <= 0) {
        return;
    }
    validationStateByBlockHash[block.hash] = state;
    if (validationStateByBlockHash.size() > 2048) {
        validationStateByBlockHash.erase(validationStateByBlockHash.begin());
    }
}

void applyRecordToCachedState(
    CBlockDB& blockDB,
    CLedgerState::state& state,
    const CFunctions::block_structure& block,
    const CFunctions::record_structure& record)
{
    if (record.hash.length() > 0) {
        state.accepted_record_hashes.insert(record.hash);
    }

    if (record.transaction_type == CFunctions::ISSUE_CURRENCY) {
        state.balances[record.recipient_public_key] += record.amount;
        state.issued_supply += record.amount;
    } else if (record.transaction_type == CFunctions::TRANSFER_CURRENCY) {
        state.balances[record.recipient_public_key] += record.amount;
        state.balances[record.sender_public_key] -= record.amount + record.fee;
        state.balances[block.creator_key] += record.fee;
        if (record.nonce > state.nonces[record.sender_public_key]) {
            state.nonces[record.sender_public_key] = record.nonce;
        }
    } else if (record.transaction_type == CFunctions::VOTE) {
        state.balances[record.sender_public_key] -= record.fee;
        state.balances[block.creator_key] += record.fee;
    } else if (record.transaction_type == CFunctions::JOIN_NETWORK) {
        if (memberIndexByKey(state.members, record.sender_public_key) < 0) {
            CLedgerState::member_state member;
            member.public_key = record.sender_public_key;
            member.name = "";
            member.join_block = block.number;
            member.last_heartbeat_block = -1;
            member.active = false;
            state.members.push_back(member);
        }
        if (record.name.length() > 0) {
            int memberIndex = memberIndexByKey(state.members, record.sender_public_key);
            if (memberIndex >= 0) {
                state.members[memberIndex].name = record.name;
                state.names[record.sender_public_key] = record.name;
            }
        }
    } else if (record.transaction_type == CFunctions::UPDATE_NAME) {
        int memberIndex = memberIndexByKey(state.members, record.sender_public_key);
        if (memberIndex >= 0) {
            state.members[memberIndex].name = record.name;
            state.names[record.sender_public_key] = record.name;
        }
    } else if (record.transaction_type == CFunctions::HEART_BEAT) {
        int memberIndex = memberIndexByKey(state.members, record.sender_public_key);
        if (memberIndex >= 0) {
            state.members[memberIndex].last_heartbeat_block = block.number;
            state.chain_has_heartbeat_records = true;
        }
    } else if (record.transaction_type == CFunctions::CARRY_FORWARD &&
               isBasicCarryForwardRecord(record, block.number) &&
               carryForwardSnapshotMatches(blockDB, record)) {
        std::string key = carryForwardKey(record);
        if (key.length() > 0 && state.accepted_carry_forward_keys.find(key) == state.accepted_carry_forward_keys.end()) {
            state.accepted_carry_forward_keys.insert(key);
            state.balances[record.sender_public_key] += CFunctions::CARRY_FORWARD_REWARD;
            state.issued_supply += CFunctions::CARRY_FORWARD_REWARD;
        }
    }
}

CLedgerState::state applyBlockToCachedState(
    CBlockDB& blockDB,
    const CLedgerState::state& parentState,
    const CFunctions::block_structure& block)
{
    CLedgerState::state state = parentState;
    if (state.first_block_id < 0) {
        state.first_block_id = block.number;
    }
    state.latest_block_id = block.number;
    state.latest_block = block;
    state.connected_block_count += 1;
    for (int i = 0; i < block.records.size(); ++i) {
        applyRecordToCachedState(blockDB, state, block, block.records.at(i));
    }
    return state;
}

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

struct BranchSnapshot {
    long block_number;
    std::string block_hash;
    std::vector<BranchMember> members;
    bool chain_has_heartbeat_records;
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

const BranchSnapshot* selectionSnapshotForBlock(const std::vector<BranchSnapshot>& snapshots, long blockNumber, long genesisBlock)
{
    if (snapshots.size() == 0) {
        return 0;
    }

    long selectionBoundary = CSelector::getSelectionBoundaryBlock(blockNumber, genesisBlock);
    const BranchSnapshot* selected = &snapshots.front();
    for (int i = 0; i < snapshots.size(); ++i) {
        if (snapshots.at(i).block_number <= selectionBoundary) {
            selected = &snapshots.at(i);
        } else {
            break;
        }
    }
    return selected;
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

bool carryForwardSnapshotMatches(CBlockDB& blockDB, const CFunctions::record_structure& record)
{
    long checkpoint = parseCarryForwardValueLong(record.value, "checkpoint");
    if (checkpoint < 0) {
        return false;
    }

    double expectedBalance = CLedgerState::balanceAtBlock(blockDB, record.recipient_public_key, checkpoint);
    return std::fabs(expectedBalance - record.amount) < 0.000001;
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
    bool enforceStateRules = false;
    CLedgerState::state parentState;
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
        CFunctions::block_structure canonicalSameSlot = blockDB.getBlock(block.number);
        bool parentIsCanonical = canonicalParent.number == parent.number && canonicalParent.hash.compare(parent.hash) == 0;
        bool sameSlotFork = canonicalSameSlot.number == block.number &&
            canonicalSameSlot.hash.length() > 0 &&
            canonicalSameSlot.hash.compare(block.hash) != 0;
        bool extendsCanonicalTip = block.previous_block_id == blockDB.getLatestBlockId();
        // Same-height fork candidates must be stored first so rebuildBestChainIndex can
        // validate and compare the full branch before changing the accepted chain.
        enforceStateRules = parentIsCanonical && sameSlotFork == false && extendsCanonicalTip;
        if (enforceStateRules) {
            if (cachedLedgerStateForBlock(parent, parentState) == false) {
                parentState = CLedgerState::build(blockDB, "", parent.number);
                cacheLedgerStateForBlock(parent, parentState);
            }
            long firstBlockId = blockDB.getFirstBlockId();
            long selectionBoundary = CSelector::getSelectionBoundaryBlock(block.number, firstBlockId);
            CLedgerState::state selectionState = CLedgerState::build(blockDB, "", selectionBoundary);
            if (selectionState.latest_block.hash.length() == 0) {
                selectionState = parentState;
            }
            std::vector<std::string> activeMemberKeys = CLedgerState::activeMemberKeysAt(selectionState, selectionState.latest_block_id);
            if (activeMemberKeys.empty()) {
                reason = "no active members can create blocks";
                return false;
            }
            std::string selectedCreator = CSelector::getSelectedUserForBlock(block.number, selectionState.latest_block.hash, activeMemberKeys);
            if (selectedCreator.compare(block.creator_key) != 0) {
                reason = "block creator is not selected for this slot";
                return false;
            }
        }
    }

    enforceStateRules = isGenesis || enforceStateRules;
    std::map<std::string, double> temporaryBalances = parentState.balances;
    std::map<std::string, long> temporaryNonces = parentState.nonces;
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
        bool duplicateRecordInParent = enforceStateRules ?
            parentState.accepted_record_hashes.find(record.hash) != parentState.accepted_record_hashes.end() :
            (!isGenesis && recordExistsInChain(blockDB, record.hash, parent.number));
        if (duplicateRecordInParent) {
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
            if (enforceStateRules && block.records_merkle_root.length() > 0) {
                long expectedNonce = temporaryNonces[record.sender_public_key] + 1;
                if (record.nonce != expectedNonce) {
                    reason = "transfer nonce is invalid";
                    return false;
                }
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
            if (enforceStateRules && block.records_merkle_root.length() > 0) {
                temporaryNonces[record.sender_public_key] = record.nonce;
            }
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

    if (enforceStateRules) {
        CLedgerState::state blockState = applyBlockToCachedState(blockDB, parentState, block);
        cacheLedgerStateForBlock(block, blockState);
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
    std::map<std::string, long> nonces;
    std::vector<BranchMember> members;
    std::map<std::string, std::string> nameOwners;
    std::set<std::string> acceptedRecordHashes;
    std::set<std::string> acceptedCarryForwardKeys;
    std::vector<BranchSnapshot> snapshots;
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

            long selectionBoundary = CSelector::getSelectionBoundaryBlock(block.number, chain.front().number);
            const BranchSnapshot* selectionSnapshot = selectionSnapshotForBlock(snapshots, block.number, chain.front().number);
            if (selectionSnapshot == 0 || selectionSnapshot->block_hash.length() == 0) {
                reason = "selection checkpoint is missing";
                return false;
            }
            std::vector<std::string> activeMemberKeys = activeMemberKeysForBlock(selectionSnapshot->members, selectionSnapshot->chain_has_heartbeat_records, selectionBoundary);
            if (activeMemberKeys.empty()) {
                reason = "no active members can create blocks";
                return false;
            }
            std::string selectedCreator = CSelector::getSelectedUserForBlock(block.number, selectionSnapshot->block_hash, activeMemberKeys);
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
                if (block.records_merkle_root.length() > 0) {
                    long expectedNonce = nonces[record.sender_public_key] + 1;
                    if (record.nonce != expectedNonce) {
                        reason = "transfer nonce is invalid";
                        return false;
                    }
                }
                double debit = record.sender_public_key.compare(record.recipient_public_key) == 0 ? record.fee : record.amount + record.fee;
                if (balances[record.sender_public_key] + 0.000001 < debit) {
                    reason = "transfer overspends sender balance";
                    return false;
                }
                balances[record.recipient_public_key] += record.amount;
                balances[record.sender_public_key] -= record.amount + record.fee;
                balances[block.creator_key] += record.fee;
                if (block.records_merkle_root.length() > 0) {
                    nonces[record.sender_public_key] = record.nonce;
                }
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

        BranchSnapshot snapshot;
        snapshot.block_number = block.number;
        snapshot.block_hash = block.hash;
        snapshot.members = members;
        snapshot.chain_has_heartbeat_records = chainHasHeartbeatRecords;
        snapshots.push_back(snapshot);
    }

    return true;
}
