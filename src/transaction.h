// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAGNITE_TRANSACTIONS_H
#define MAGNITE_TRANSACTIONS_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

class CTransaction
{
private:


public:
    //! Construct an empty wallet.
    CTransaction()
    {
    }

    //! Destructor.
    ~CTransaction()
    {
    }

    std::string joinNetwork(std::string publicKey);
    
    std::string sendPayment(std::string privateKey, std::string publicKey, std::string toAddress, double amount, int id);

    std::string sendCurrencyIssuance(std::string privateKey, std::string publicKey, std::string toAddress, double amount, int id);

    bool verifyPayment(std::string message);

    // TODO Parse messages 
};

#endif // MAGNITE_TRANSACTION_H
