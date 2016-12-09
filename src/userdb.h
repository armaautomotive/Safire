// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAGNITE_USER_DB_H
#define MAGNITE_USER_DB_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

class CUserDB
{
private:


public:
    //! Construct an invalid private key.
    CUserDB()
    {
    }

    //! Destructor (again necessary because of memlocking).
    ~CUserDB()
    {
    }

    int AddUser(std::string publicKey);
    void GetUsers();

};

#endif // MAGNITE_USER_DB_H
