// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAGNITE_RSA_CRYPTO_H
#define MAGNITE_RSA_CRYPTO_H

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

class CRSACrypto
{
private:
    //! Whether this private key is valid. We check for correctness when modifying the key
    //! data, so fValid should always correspond to the actual state.
    bool fValid;


public:
    //! Construct an invalid private key.
    CRSACrypto() 
    {
    }

    //! Destructor (again necessary because of memlocking).
    ~CRSACrypto()
    {
    }

    int GetKeyPair(std::string & privateKey, std::string & publicKey);

};

#endif // MAGNITE_RSA_CRYPTO_H
