/**
* CSelector
* 
* The selector defines a network wide mechanism for identifying a single user 
* from the network durring a given block of time. This selected user can be granted authenticity
* by other piers for blocks submitted.
*/

// Copyright (c) 2016 2017 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/selector.h"
#include "global.h"
#include "ecdsacrypto.h"
#include <cstdlib>
#include "networktime.h"
#include "userdb.h"

std::vector< std::string > CSelector::users;

namespace {

int hexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return 0;
}

} // namespace


/**
* syncronizeTime
*
* Description: calculate time offset between local machine and network. 
* Apply time difference when using local time so that the network is syncronized.
*/
void CSelector::syncronizeTime(){
    // Peer-based time synchronization is handled by CLocalPeerClient.
}

/**
 *
 *
 */
long CSelector::getCurrentTimeBlock(){
    CNetworkTime netTime;
    long timeBlock = (long)(netTime.getEpoch() / 15);
    return timeBlock; 
}

/**
 * getSelectedUser
 *
 * Description: for a given time block, calculate and return the user key
 *  assigned to generate the block.
 *  Used to determine if a user is the current block creator and to validate past
 *  block creators.
 *
 *  The selected user is a function of the block time as a hash to index from the
 *  available users. Available users have a membership record in the chain and a heartbeat
 *  record with a recent (DEFINE) block index indicating they are up to date and able to build
 *  blocks.
 *
 * @param long time, defines which time block is being requested.
 *          (current for new blocks and past for chain validation.)
 * @return std::string user public key.
 */
std::string CSelector::getSelectedUser(long time){
    return getSelectedUserForBlock(time, "", users);
}

long CSelector::getEpochSizeBlocks()
{
    return 100;
}

long CSelector::getSelectionLagEpochs()
{
    return 2;
}

long CSelector::getRecoveryWindowBlocks()
{
    return 10;
}

long CSelector::getEpochForBlock(long blockNumber, long genesisBlock)
{
    if (genesisBlock <= 0 || blockNumber <= genesisBlock) {
        return 0;
    }
    return (blockNumber - genesisBlock) / getEpochSizeBlocks();
}

long CSelector::getSelectionBoundaryBlock(long blockNumber, long genesisBlock)
{
    if (genesisBlock <= 0) {
        return blockNumber;
    }

    long currentEpoch = getEpochForBlock(blockNumber, genesisBlock);
    if (currentEpoch < getSelectionLagEpochs()) {
        return genesisBlock;
    }

    long selectionEpoch = currentEpoch - getSelectionLagEpochs();
    return genesisBlock + ((selectionEpoch + 1) * getEpochSizeBlocks()) - 1;
}

long CSelector::getSelectedIndexForBlock(long blockNumber, const std::string& selectionSeedHash, long userCount)
{
    if (userCount <= 0) {
        return -1;
    }

    std::stringstream seed;
    seed << selectionSeedHash << ":" << blockNumber;

    CECDSACrypto ecdsa;
    char hash[65];
    std::string seedString = seed.str();
    ecdsa.sha256((char*)seedString.c_str(), hash);

    unsigned long long value = 0;
    for (int i = 0; i < 16 && hash[i] != '\0'; ++i) {
        value = (value << 4) + hexValue(hash[i]);
    }
    return static_cast<long>(value % static_cast<unsigned long long>(userCount));
}

std::string CSelector::getSelectedUserForBlock(long blockNumber, const std::string& selectionSeedHash, const std::vector<std::string>& activeUsers)
{
    long index = getSelectedIndexForBlock(blockNumber, selectionSeedHash, activeUsers.size());
    if (index < 0 || index >= activeUsers.size()) {
        return "";
    }
    return activeUsers.at(index);
}

std::string CSelector::getAuthorizedCreatorForBlock(
    long blockNumber,
    const std::string& selectionSeedHash,
    const std::vector<std::string>& checkpointActiveUsers,
    const std::string& latestSeedHash,
    const std::vector<std::string>& latestActiveUsers,
    const std::string& recoveryPublicKey,
    long consecutiveRecoveryBlocks,
    bool* recoveryMode,
    bool* latestStateFallback)
{
    if(recoveryMode){
        *recoveryMode = false;
    }
    if(latestStateFallback){
        *latestStateFallback = false;
    }

    if(checkpointActiveUsers.size() > 0 && selectionSeedHash.length() > 0){
        return getSelectedUserForBlock(blockNumber, selectionSeedHash, checkpointActiveUsers);
    }

    if(latestActiveUsers.size() > 0 && latestSeedHash.length() > 0){
        if(latestStateFallback){
            *latestStateFallback = true;
        }
        return getSelectedUserForBlock(blockNumber, latestSeedHash, latestActiveUsers);
    }

    if(recoveryPublicKey.length() > 0 &&
       consecutiveRecoveryBlocks < getRecoveryWindowBlocks()){
        if(recoveryMode){
            *recoveryMode = true;
        }
        return recoveryPublicKey;
    }

    return "";
}

/**
 * isSelected -
 *  DEPRICATE replace with or call getSelectedUser(time)
 *
 * Description: is a given user designated for the current time period.
 *       - a user can only be selected if they have a recent heartbeat, or there would be no other user?
 *
 * @param string users public key for identification.
 */
bool CSelector::isSelected(std::string publicKey){
    CUserDB userDb;
    // userDb.GetUserCount();
    
    if(users.size() == 0){ // not loaded yet
        return false;
    }

    CNetworkTime netTime;
    //std::stringstream ss;
    //ss << timev;
    //std::string ts = ss.str();

    // TODO: get time from central or synced set of servers for syncronization.

    long timeBlock = (long)(netTime.getEpoch() / 15);
    std::string selectedUser = getSelectedUserForBlock(timeBlock, "", users);
    //std::cout << " time " <<  "  " << timeBlock << " mod " << userIndex <<  " users " << users.size() << " " << selectedUser <<  std::endl; 

    // Current time to 30 seconds, hashed into one of the user set. 
    // If that user is the current user return true;

    // for(int i = 0; i < users.size(); i++){ 
    if(selectedUser.compare(publicKey) == 0){
        return true;
    }
    return false;
}


/**
*
*
* TODO: use hashmap instead of vector
*/
void CSelector::addUser(std::string user){
    //std::cout << " add " << user << std::endl;
    bool exists = false;
    for(int i = 0; i < CSelector::users.size(); i++){
        std::string existingUser = (std::string)CSelector::users[i];
        if( existingUser.compare(user) == 0 ){
            exists = true;
        }
    }
    if(exists == false){
        CSelector::users.push_back(user);
    }    
}
