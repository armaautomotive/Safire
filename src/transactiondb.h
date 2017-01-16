// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAGNITE_TRANSACTION_DB_H
#define MAGNITE_TRANSACTION_DB_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

class CTransactionDB
{
private:


public:
    //! 
    CTransactionDB()
    {
    }

    //! 
    ~CTransactionDB()
    {
    }

    struct transaction {
        std::string id;
        std::string message;
        std::string x;
    };

    int AddTransaction(std::string id, std::string message);
    int AddTransaction(CTransactionDB::transaction transaction);
    void GetTransactions();
    int GetTransactionCount();
};

#endif // MAGNITE_TRANSACTION_DB_H
