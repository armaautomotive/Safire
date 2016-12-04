
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include "crypto/common.h"
#include "crypto/sha256.h"
#include "crypto/hmac_sha256.h"
#include "utilstrencodings.h"

//#include "wallet/wallet.h"
//#include "key.h"
//#include "pubkey.h"

//#include <openssl/crypto.h> // no worky
//#include <openssl/ec.h>
#include <stdio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

//#include "rsacrypto.h"
#include "ecdsacrypto.h"
#include "wallet.h"

static const uint64_t BUFFER_SIZE = 1000*1000; // Temp

bool is_file_exist(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

int main()
{
    std::cout << "Magnite Digital Currency v0.0.1" << std::endl;

    std::string p;
    std::string v;
    std::string u;
    //std::cout << "  " << std::endl;	
    CECDSACrypto ecdsa;
    int r = ecdsa.GetKeyPair(p, v, u );
    //std::cout << "  private  " << p << "\n  public " << v << "\n  Public compressed: " << u << std::endl; 
    ecdsa.runTests();

    CWallet wallet;
    bool e = wallet.fileExists("wallet.dat");
    printf(" wallet exists: %d  \n", e);
    if(e == 0){
        printf("No wallet found. Creating a new one...\n");
        std::string privateKey;
        std::string publicKey;
        std::string publicKeyUncompressed;
        int r = ecdsa.RandomPrivateKey(privateKey);
        r = ecdsa.GetPublicKey(privateKey, publicKeyUncompressed, publicKey);        
         
        wallet.write(privateKey, publicKey);

    } else {
        // Load wallet
        std::string privateKey;
        std::string publicKey;
        wallet.read(privateKey, publicKey);
  
        std::cout << "  private  " << privateKey << "\n  public " << publicKey << "\n " << std::endl; 

    }
	
    std::cout << " Done " << std::endl;
}
