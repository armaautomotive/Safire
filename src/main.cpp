
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include "crypto/common.h"
#include "crypto/sha256.h"

//#include "wallet/wallet.h"
#include "support/cleanse.h"
#include "key.h"
#include "pubkey.h"

static const uint64_t BUFFER_SIZE = 1000*1000; // Temp

/*
CPubKey CWallet::GenerateNewKey()
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    CKey secret;

    // Create new metadata
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // use HD key derivation if HD was enabled during wallet creation
    if (IsHDEnabled()) {
        DeriveNewChildKey(metadata, secret);
    } else {
        secret.MakeNewKey(fCompressed);
    }

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    mapKeyMetadata[pubkey.GetID()] = metadata;
    if (!nTimeFirstKey || nCreationTime < nTimeFirstKey)
        nTimeFirstKey = nCreationTime;

    if (!AddKeyPubKey(secret, pubkey))
        throw std::runtime_error(std::string(__func__) + ": AddKey failed");
    return pubkey;
}
*/

int main()
{
	std::cout << "Maple Digital Currency v0.0.1" << std::endl;

	//sha256::
	//CSHA256 sha;
	uint8_t hash[CSHA256::OUTPUT_SIZE];
	std::vector<uint8_t> in(BUFFER_SIZE, 0);
	//while (state.KeepRunning())
        //CSHA256().Write(begin_ptr(in), in.size()).Finalize(hash);

	std::string message("Test");

	CKey secret;

	//CPubKey pub


}

