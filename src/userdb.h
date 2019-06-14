// Copyright (c) 2016 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_USER_DB_H
#define SAFIRE_USER_DB_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "functions/functions.h"
#include "leveldb/db.h"

class CUserDB
{
private:
    static leveldb::DB * db;

public:
    //! Construct an invalid private key.
    CUserDB();
    ~CUserDB();

    //struct user { // depricate
    //    long position;
    //    std::string publicKey;
    //    std::string ipAddress;
    //};
    
    leveldb::DB * getDatabase();
    //leveldb::DB * getDatabase2();

    void setScannedBlockId(long number);
    long getScannedBlockId();
    
    void setUser(CFunctions::user_structure user);
    CFunctions::user_structure getUser(std::string public_key);
    
    std::vector<CFunctions::user_structure> getUsers();
    
    void DeleteIndex();
    
    long getUserCount();
    void setUserCount(long count);
    
    // depricate
    /*
    int AddUser(long position, std::string publicKey, std::string ipAddress);
    int AddUser(CUserDB::user user);
    void GetUsers();
    int GetUsersCount();
    
    long GetLastUserPosition();
    */
};

#endif // SAFIRE_USER_DB_H
