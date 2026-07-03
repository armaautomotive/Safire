// Copyright (c) 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_SELECTOR_H
#define SAFIRE_SELECTOR_H

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>

class CSelector
{
private:
    int secondsPerBlock = 15;
public:
    //! Construct
    CSelector()
    {
    }

    //! Destructor
    ~CSelector()
    {
    }

    void syncronizeTime();
    bool isSelected(std::string publicKey);
    std::string getSelectedUser(long time);
    static long getEpochSizeBlocks();
    static long getSelectionLagEpochs();
    static long getEpochForBlock(long blockNumber, long genesisBlock);
    static long getSelectionBoundaryBlock(long blockNumber, long genesisBlock);
    static long getRecoveryWindowBlocks();
    static long getSelectedIndexForBlock(long blockNumber, const std::string& selectionSeedHash, long userCount);
    static std::string getSelectedUserForBlock(long blockNumber, const std::string& selectionSeedHash, const std::vector<std::string>& activeUsers);
    static std::string getAuthorizedCreatorForBlock(
        long blockNumber,
        const std::string& selectionSeedHash,
        const std::vector<std::string>& checkpointActiveUsers,
        const std::string& latestSeedHash,
        const std::vector<std::string>& latestActiveUsers,
        const std::string& recoveryPublicKey,
        long consecutiveRecoveryBlocks,
        bool* recoveryMode = 0,
        bool* latestStateFallback = 0);
    long getCurrentTimeBlock();
    void addUser(std::string user);

    static std::vector< std::string > users;
};

#endif // SAFIRE_SELECTOR_H
