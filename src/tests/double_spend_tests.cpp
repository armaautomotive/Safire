#include "ecdsacrypto.h"
#include "functions/chainvalidator.h"
#include "functions/functions.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct TestKey {
    std::string private_key;
    std::string public_key;
};

TestKey makeKey(CECDSACrypto& ecdsa)
{
    TestKey key;
    std::string public_key_uncompressed;
    ecdsa.RandomPrivateKey(key.private_key);
    ecdsa.GetPublicKey(key.private_key, public_key_uncompressed, key.public_key);
    return key;
}

CFunctions::record_structure signedRecord(
    CFunctions& functions,
    CECDSACrypto& ecdsa,
    const std::string& privateKey,
    CFunctions::record_structure record)
{
    record.hash = functions.getRecordHash(record);
    ecdsa.SignMessage(privateKey, record.hash, record.signature);
    return record;
}

CFunctions::block_structure signedBlock(
    CFunctions& functions,
    CECDSACrypto& ecdsa,
    const std::string& privateKey,
    CFunctions::block_structure block)
{
    block.records_merkle_root = functions.getRecordsMerkleRoot(block.records);
    block.hash = functions.getBlockHash(block);
    ecdsa.SignMessage(privateKey, block.hash, block.signature);
    return block;
}

CFunctions::record_structure joinRecord(CFunctions& functions, CECDSACrypto& ecdsa, const TestKey& key)
{
    CFunctions::record_structure record;
    record.network = "main";
    record.time = "1";
    record.transaction_type = CFunctions::JOIN_NETWORK;
    record.amount = 0.0;
    record.fee = 0.0;
    record.sender_public_key = key.public_key;
    record.name = "tester";
    return signedRecord(functions, ecdsa, key.private_key, record);
}

CFunctions::record_structure issueRecord(CFunctions& functions, CECDSACrypto& ecdsa, const TestKey& key, const std::string& time)
{
    CFunctions::record_structure record;
    record.network = "main";
    record.time = time;
    record.transaction_type = CFunctions::ISSUE_CURRENCY;
    record.amount = 1.0;
    record.fee = 0.0;
    record.sender_public_key = key.public_key;
    record.recipient_public_key = key.public_key;
    return signedRecord(functions, ecdsa, key.private_key, record);
}

CFunctions::record_structure transferRecord(
    CFunctions& functions,
    CECDSACrypto& ecdsa,
    const TestKey& sender,
    const TestKey& recipient,
    long nonce,
    double amount,
    const std::string& time)
{
    CFunctions::record_structure record;
    record.network = "main";
    record.time = time;
    record.transaction_type = CFunctions::TRANSFER_CURRENCY;
    record.nonce = nonce;
    record.amount = amount;
    record.fee = 0.0;
    record.sender_public_key = sender.public_key;
    record.recipient_public_key = recipient.public_key;
    return signedRecord(functions, ecdsa, sender.private_key, record);
}

CFunctions::block_structure genesisBlock(CFunctions& functions, CECDSACrypto& ecdsa, const TestKey& creator)
{
    CFunctions::block_structure block;
    block.network = "main";
    block.number = 100;
    block.time = "1";
    block.previous_block_id = -1;
    block.creator_key = creator.public_key;
    block.records.push_back(joinRecord(functions, ecdsa, creator));
    block.records.push_back(issueRecord(functions, ecdsa, creator, "1"));
    return signedBlock(functions, ecdsa, creator.private_key, block);
}

CFunctions::block_structure childBlock(
    CFunctions& functions,
    CECDSACrypto& ecdsa,
    const TestKey& creator,
    const CFunctions::block_structure& parent,
    long number,
    const std::vector<CFunctions::record_structure>& records)
{
    CFunctions::block_structure block;
    block.network = "main";
    block.number = number;
    block.time = "2";
    block.previous_block_id = parent.number;
    block.previous_block_hash = parent.hash;
    block.creator_key = creator.public_key;
    block.records = records;
    return signedBlock(functions, ecdsa, creator.private_key, block);
}

void requireValid(const std::vector<CFunctions::block_structure>& chain, const std::string& label)
{
    std::string reason;
    if (!CChainValidator::validateConnectedChain(chain, reason)) {
        std::cerr << label << " should be valid, got: " << reason << std::endl;
        std::exit(1);
    }
}

void requireInvalid(const std::vector<CFunctions::block_structure>& chain, const std::string& label)
{
    std::string reason;
    if (CChainValidator::validateConnectedChain(chain, reason)) {
        std::cerr << label << " should be invalid" << std::endl;
        std::exit(1);
    }
}

}

int main()
{
    CFunctions functions;
    CECDSACrypto ecdsa;
    TestKey sender = makeKey(ecdsa);
    TestKey recipient = makeKey(ecdsa);

    CFunctions::block_structure genesis = genesisBlock(functions, ecdsa, sender);

    CFunctions::record_structure spend1 = transferRecord(functions, ecdsa, sender, recipient, 1, 0.4, "2");
    CFunctions::block_structure validSpend = childBlock(functions, ecdsa, sender, genesis, 101, std::vector<CFunctions::record_structure>(1, spend1));
    requireValid(std::vector<CFunctions::block_structure>{genesis, validSpend}, "single nonce-1 spend");

    CFunctions::record_structure duplicateNonce = transferRecord(functions, ecdsa, sender, recipient, 1, 0.2, "2");
    std::vector<CFunctions::record_structure> duplicateRecords;
    duplicateRecords.push_back(spend1);
    duplicateRecords.push_back(duplicateNonce);
    CFunctions::block_structure duplicateBlock = childBlock(functions, ecdsa, sender, genesis, 101, duplicateRecords);
    requireInvalid(std::vector<CFunctions::block_structure>{genesis, duplicateBlock}, "two different transfers with nonce 1");

    CFunctions::record_structure skippedNonce = transferRecord(functions, ecdsa, sender, recipient, 2, 0.2, "2");
    CFunctions::block_structure skippedNonceBlock = childBlock(functions, ecdsa, sender, genesis, 101, std::vector<CFunctions::record_structure>(1, skippedNonce));
    requireInvalid(std::vector<CFunctions::block_structure>{genesis, skippedNonceBlock}, "first transfer starts at nonce 2");

    CFunctions::record_structure spend2 = transferRecord(functions, ecdsa, sender, recipient, 2, 0.2, "3");
    CFunctions::block_structure validSpend2 = childBlock(functions, ecdsa, sender, validSpend, 102, std::vector<CFunctions::record_structure>(1, spend2));
    requireValid(std::vector<CFunctions::block_structure>{genesis, validSpend, validSpend2}, "nonce 1 then nonce 2");

    CFunctions::block_structure replayBlock = childBlock(functions, ecdsa, sender, validSpend, 102, std::vector<CFunctions::record_structure>(1, spend1));
    requireInvalid(std::vector<CFunctions::block_structure>{genesis, validSpend, replayBlock}, "replayed nonce-1 spend");

    std::cout << "Double-spend nonce tests passed." << std::endl;
    return 0;
}
