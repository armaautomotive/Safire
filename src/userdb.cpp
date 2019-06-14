// Copyright (c) 2016, 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "userdb.h"
#include <sstream>
#include <unistd.h>   // open and close
#include "leveldb/db.h"
#include "platform.h"
#include "log.h";
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "functions/functions.h"

using namespace std;

leveldb::DB * CUserDB::db;

CUserDB::CUserDB(){
}

CUserDB::~CUserDB(){
}

/**
 * getDatabase
 *
 * Description: Get a levelDB instance.
 */
leveldb::DB * CUserDB::getDatabase(){
    if(CUserDB::db){
        return CUserDB::db;
    }
    
    leveldb::DB* ldb;
    
    CPlatform platform;
    std::string dbPath = platform.getSafirePath();
    //std::cout << dbPath << std::endl;
    
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./userdb", &ldb);
    if (false == status.ok())
    {
        cerr << "Unable to open/create database './userdb'" << endl;
        cerr << status.ToString() << endl;
        //return 0;
        //CBlockDB::db = 0;
        ldb = 0;
    }
    
    CUserDB::db = ldb;
    
    return ldb;
}


/**
 * setScannedBlockId
 *
 * Description: Save the last block to be scanned. Saves the local chain from having to be rescanned
 *  repeatedly.
 *  the chain.
 * @param long number - block number.
 */
void CUserDB::setScannedBlockId(long number){
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    ostringstream keyStream;
    keyStream << "scanned_block_id";
    ostringstream valueStream;
    valueStream << boost::lexical_cast<std::string>(number);
    db->Put(writeOptions, keyStream.str(), valueStream.str());
    //delete db;
}

/**
 * getScannedBBlockId
 *
 * Description:
 *  block number.
 * @return long block number.
 */
long CUserDB::getScannedBlockId(){
    //std::cout << " a \n ";
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    std::string key = "scanned_block_id"; // boost::lexical_cast<std::string>(number);
    std::string scannedBlockIdString;
    db->Get(leveldb::ReadOptions(), key, &scannedBlockIdString);
    
    //delete db;
    
    long result = -1;
    //std::stol(firstBlockId);
    if(scannedBlockIdString.length() > 0){
        result = boost::lexical_cast<long>(scannedBlockIdString);
    }
    
    //std::cout << " z \n ";
    return result;
}

/**
 * AddUser
 *
 * Description: Add a user record to store membership, key and current balance for all users.
 *  Used to verify transfer payments have sufficient balance
 *  as well as verification of destination addresses before sending funds.
 *
 * TODO: add to /dao/UserDB ???
 */
void CUserDB::setUser(CFunctions::user_structure user){
    CFunctions functions;
    CFileLogger log;
    
    leveldb::WriteOptions writeOptions;
    leveldb::DB * db = getDatabase();
    if(!db){
        std::cout << "Error: CBlockDB::setUser \n";
        return;
    }
    
    // Insert block record into leveldb
    ostringstream keyStream;
    keyStream << "u_" << user.public_key;
    ostringstream valueStream;
    
    std::string json = functions.userJSON(user);
    valueStream << json;
    db->Put(writeOptions, keyStream.str(), valueStream.str());
}

/**
 * getUser
 *
 * Description:
 */
CFunctions::user_structure CUserDB::getUser(std::string public_key){
    CFunctions::user_structure user;
    user.public_key = -1;
    
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    
    std::string key = "u_" + boost::lexical_cast<std::string>(public_key);
    std::string userJson;
    db->Get(leveldb::ReadOptions(), key, &userJson);
    
    CFunctions functions;
    user = functions.parseUserJson(userJson);
    
    // Close the database
    //delete db;
    return user;
}

/**
 * getUsers
 *
 * Description: Return a list of user structs in a vector.
 */
std::vector<CFunctions::user_structure> CUserDB::getUsers(){
    std::vector<CFunctions::user_structure> users;
    
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    
    CFunctions functions;
    
    // Iterate over each item in the database and print them
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        //cout << it->key().ToString() << " : " << it->value().ToString() << endl;
        CFunctions::user_structure user;
        user = functions.parseUserJson(it->value().ToString());
        users.push_back(user);
    }
    if (false == it->status().ok())
    {
        cerr << "An error was found during the scan" << endl;
        cerr << it->status().ToString() << endl;
    }
    return users;
}

/**
 * DeleteIndex
 *
 * Description: Delete index data containing user values accumulated
 *  from the chain.
 */
void CUserDB::DeleteIndex(){
    // Delete index data
    leveldb::WriteOptions writeOptions;
    leveldb::DB* db = getDatabase();
    // Iterate over each item in the database and print them
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        //cout << it->key().ToString() << " : " << it->value().ToString() << endl;
        if( boost::starts_with(it->key().ToString(), "u_") ){
            db->Delete(leveldb::WriteOptions(), it->key().ToString());
            cout << "Delete: " << it->key().ToString() << "\n";
        }
    }
    if (false == it->status().ok())
    {
        cerr << "An error was found during the scan" << endl;
        cerr << it->status().ToString() << endl;
    }
    delete it;
    
    // reset scan index.
    setScannedBlockId(0);
}


/**
 * GetUserCount
 *
 * Description: retrieve user count.
 */
long CUserDB::getUserCount(){
    
    
    return 0;
}

/**
 *
 *
 */
void CUserDB::setUserCount(long count){
    
}


/**
* RandomPrivateKey
*
* Description: Generate a random sha256 hash to be used as a private key.
*     Generates 128 random ascii characters to feed into a sha256 hash.
*
* @param: long position number of users accepted
* @param: std::string output private key.
* @param: std::string ipAddress user node address. 
* @return int returns 1 is successfull.
*/
/*
int CUserDB::AddUser(long position, std::string publicKey, std::string ipAddress)
{
    user u;
    u.position = position;
    u.publicKey = publicKey;
    u.ipAddress = ipAddress;
    return AddUser(u);
}
 */

/**
* AddUser
*
* Description: 
*
* @param: CUserDB::user
* @return: int
*/
/*
int CUserDB::AddUser(CUserDB::user user){
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./userdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './userdb'" << endl;
        cerr << status.ToString() << endl;
        return -1;
    }

    leveldb::WriteOptions writeOptions;

    // Insert user
    ostringstream keyStream;
    keyStream << user.publicKey;
    ostringstream valueStream;
    valueStream << user.position << user.publicKey << user.ipAddress;
    db->Put(writeOptions, keyStream.str(), valueStream.str());

    // Save the position
    ostringstream keyStreamPos;   
    keyStreamPos << "lastPosition";
    ostringstream valueStreamPos;
  

    // Close the database
    delete db;
    return 1;
}
*/

/**
* GetLastUserPosition
* Description: Get a cached copy of the last added user position. 
*  
*/
/*
long CUserDB::GetLastUserPosition(){

    return 0;
} 


void CUserDB::GetUsers()
{
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./userdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './userdb'" << endl;
        cerr << status.ToString() << endl;
        return;
    }


    // Iterate over each item in the database and print them
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        cout << it->key().ToString() << " : " << it->value().ToString() << endl;
    }    
    if (false == it->status().ok())
    {
        cerr << "An error was found during the scan" << endl;
        cerr << it->status().ToString() << endl; 
    }

    

    // Close the database
    delete db; 
}
 */
