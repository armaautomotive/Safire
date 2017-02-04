#include "userdb.h"

#include <sstream>
#include <unistd.h>   // open and close
#include "leveldb/db.h"

using namespace std;

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
int CUserDB::AddUser(long position, std::string publicKey, std::string ipAddress)
{
    user u;
    u.position = position;
    u.publicKey = publicKey;
    u.ipAddress = ipAddress;
    return AddUser(u);
}

/**
* AddUser
*
* Description: 
*
* @param: CUserDB::user
* @return: int
*/
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


/**
* GetLastUserPosition
* Description: Get a cached copy of the last added user position. 
*  
*/
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


int CUserDB::GetUsersCount(){

	return 0;
} 
