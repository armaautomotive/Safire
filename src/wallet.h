// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAGNITE_WALLET_H
#define MAGNITE_WALLET_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

class CWallet
{
private:


public:
    //! Construct an empty wallet.
    CWallet()
    {
    }

    //! Destructor.
    ~CWallet()
    {
    }

    bool fileExists(std::string fileName);
    bool write(std::string privateKey, std::string publicKey);
    bool read(std::string & privateKey, std::string & publicKey);
};

#endif // MAGNITE_WALLET_H

