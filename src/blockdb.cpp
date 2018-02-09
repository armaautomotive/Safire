#include "blockdb.h"

#include <sstream>
#include <unistd.h>   // open and close
#include "leveldb/db.h"
#include "platform.h"
#include <boost/lexical_cast.hpp>

using namespace std;

/**
* RandomPrivateKey
*
* Description: Generate a random sha256 hash to be used as a private key.
*     Generates 128 random ascii characters to feed into a sha256 hash.
*
* @param: std::string output private key.
* @return bool returns 1 is successfull.
*/
bool CBlockDB::AddBlock(CFunctions::block_structure block){
    CPlatform platform;
    CFunctions functions;
    std::string dbPath = platform.getSafirePath();
    //std::cout << dbPath << std::endl;

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
    keyStream << boost::lexical_cast<std::string>(block.number);
    ostringstream valueStream;
    valueStream << functions.blockJSON(block);
    db->Put(writeOptions, keyStream.str(), valueStream.str());

    // Close the database
    delete db;
    return true;
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


/**
* getBlock
*
* Description: Get a block by number 
*/
CFunctions::block_structure CBlockDB::getBlock(long number){
    CFunctions::block_structure block;
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





    return block;
}

