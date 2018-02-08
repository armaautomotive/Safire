#include "blockdb.h"

#include <sstream>
#include <unistd.h>   // open and close
#include "leveldb/db.h"
#include "platform.h"

using namespace std;

/**
* RandomPrivateKey
*
* Description: Generate a random sha256 hash to be used as a private key.
*     Generates 128 random ascii characters to feed into a sha256 hash.
*
* @param: std::string output private key.
* @return int returns 1 is successfull.
*/
int CBlockDB::AddBlock(std::string publicKey)
{
    CPlatform platform;
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './blockdb'" << endl;
        cerr << status.ToString() << endl;
        return -1;
    }

    leveldb::WriteOptions writeOptions;

    // Insert
    ostringstream keyStream;
    keyStream << "Key" << publicKey;
    ostringstream valueStream;
    valueStream << "Test data value: " << publicKey;
    db->Put(writeOptions, keyStream.str(), valueStream.str());

    

    // Close the database
    delete db;
    return 1;
}


void CBlockDB::GetBlocks()
{
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "./blockdb", &db);
    if (false == status.ok())
    {
        cerr << "Unable to open/create test database './blockdb'" << endl;
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
