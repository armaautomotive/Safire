
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
#include "transaction.h"

#include "userdb.h"
#include "leveldb/db.h" // TEMP 
#include "blockdb.h"

//static const uint64_t BUFFER_SIZE = 1000*1000; // Temp
using namespace std;

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

    std::string privateKey;
    std::string publicKey;

    CWallet wallet;
    bool e = wallet.fileExists("wallet.dat");
    printf(" wallet exists: %d  \n", e);
    if(e == 0){
        printf("No wallet found. Creating a new one...\n");
        std::string publicKeyUncompressed;
        int r = ecdsa.RandomPrivateKey(privateKey);
        r = ecdsa.GetPublicKey(privateKey, publicKeyUncompressed, publicKey);        
        wallet.write(privateKey, publicKey);
    } else {
        // Load wallet
        wallet.read(privateKey, publicKey);
        std::cout << "  private  " << privateKey << "\n  public " << publicKey << "\n " << std::endl; 
    }

    // Transactions
    CTransaction transaction;
    std::string join = transaction.joinNetwork(publicKey);
    std::cout << " join: " << join << std::endl;	
    std::string sendPayment = transaction.sendPayment( privateKey, publicKey, u, 23.45, 1);
    std::cout << " payment: " << sendPayment << std::endl;

    
    CUserDB userDB;
    userDB.AddUser("test");
    userDB.GetUsers(); 


    CBlockDB blockDB;
    blockDB.AddBlock("First");
    blockDB.GetBlocks();


    std::cout << " Done " << std::endl;
}
