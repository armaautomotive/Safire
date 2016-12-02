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
#include <openssl/pem.h>
#include <openssl/crypto.h>

typedef unsigned char byte;
#define UNUSED(x) ((void)x)
const char hn[] = "SHA256";

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
    int SignMessage(std::string & privateKey, std::string & message, std::string & signature);    
    int verify(std::string message, std::string sig, std::string pkey);

    int make_keys(EVP_PKEY** skey, EVP_PKEY** vkey);
    int sign_it(const byte* msg, size_t mlen, byte** sig, size_t* slen, EVP_PKEY* pkey);
    int verify_it(const byte* msg, size_t mlen, const byte* sig, size_t slen, EVP_PKEY* pkey);
    void print_it(const char* label, const byte* buff, size_t len); // Private?

    void DataToString(const byte * buff, size_t buff_len, std::string & str);
    void StringToData(std::string & str, const byte * buff, int * buff_len);

    unsigned char gethex(const char *s, char **endptr);
    unsigned char * convert(const char *s, int *length);
};

#endif // MAGNITE_RSA_CRYPTO_H
