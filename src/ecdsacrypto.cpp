// Copyright (c) 2016 Jon Taylor 
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ecdsacrypto.h"

#include <sstream>
#include <unistd.h>   // open and close
#include <sys/stat.h> // temp because we removed util
#include <fcntl.h> // temp removed util.h
#include <time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/**
* RandomPrivateKey
*
* Description: Generate a random sha256 hash to be used as a private key.
*     Generates 128 random ascii characters to feed into a sha256 hash.
*
* @param: std::string output private key.
* @return int returns 1 is successfull.
*/
int CECDSACrypto::RandomPrivateKey(std::string & privateKey)
{
    char random[129];
    srand(time(NULL));
    for(int i = 0; i < 128; i++){
       int digit = rand() % 74;
       random[i] = (char)48 + digit;
    }
    random[128] = 0;
    //printf("    random seed: %s \n", random);
    char * privateHex = (char *)malloc( 65 );
    char c_rand[65];
    c_rand[64] = 0;
    sha256((char *)random, privateHex); 
    //printf("    Random Key: %s\n", privateHex);
    privateKey = std::string(privateHex);
    free(privateHex);
    return 1;
}


/**
* GetPublicKey
*
* Description: Given a private key, derive the public key from it. Results in a 128 character hex encoded string.
*
* @param: std::string privateKey - input a private key string in hex to process.
* @param: std::string publicKey - output a string public key for a given private key.
*/
int CECDSACrypto::GetPublicKey(std::string & privateKey, std::string & publicKey)
{
    EC_KEY *eckey = NULL;
    EC_POINT *pub_key = NULL;
    const EC_GROUP *group = NULL;
    BIGNUM start;
    BIGNUM *res;
    BN_CTX *ctx;

    BN_init(&start);
    ctx = BN_CTX_new(); // ctx is an optional buffer to save time from allocating and deallocating memory whenever required

    res = &start;
    //     BN_hex2bn(&res,"3D79F601620A6D05DB7FED883AB8BCD08A9101B166BC60166869DA5FC08D936E");
    BN_hex2bn(&res, privateKey.c_str());
    eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    group = EC_KEY_get0_group(eckey);
    pub_key = EC_POINT_new(group);

    EC_KEY_set_private_key(eckey, res);

    /* pub_key is a new uninitialized `EC_POINT*`.  priv_key res is a `BIGNUM*`. */
    if (!EC_POINT_mul(group, pub_key, res, NULL, NULL, ctx)){
       printf("Error at EC_POINT_mul.\n");
    }

    //     assert(EC_POINT_bn2point(group, &res, pub_key, ctx)); // Null here
    EC_KEY_set_public_key(eckey, pub_key);
    char *cc = EC_POINT_point2hex(group, pub_key, (point_conversion_form_t)4, ctx);
    char *c=cc;

    //BIGNUM * bignum = BN_new();
    //BN_init(&bignum);
    //bignum = EC_POINT_point2bn(group, pub_key, (point_conversion_form_t)4, bignum, ctx);    
    // free bignum 

    publicKey = std::string(cc);
     
    BN_CTX_free(ctx);
    free(cc);
    return 1;
}


/**
* SignMessage
*
* Description: Given a private key and message, sha256 the message and sign it with the private key
*  resulting in a signature.
*
* @param std::string private key - input private key hex string.
* @param std::string message - input message to create a signature for.
* @param std::string signature - output signature string in hex format. Concatinated numbers.
* @return int - 1 if success.
*/
int CECDSACrypto::SignMessage(std::string privateKey, std::string message, std::string & signature)
{
    //if( privateKey == NULL || message == NULL ){
    //   return 0;
    //}
    //printf("SignMessage\n");
    //printf("    Private: %s  \n", std::string(privateKey).c_str() );
    //printf("    Message: %s  \n", std::string(message).c_str() );
 
    unsigned char * uc_digest = usha256((char *)message.c_str());
    unsigned char c_digest[33];
    char d[33]; 
    sha256((char *)message.c_str(), (char*)d);
    //c_digest =  reinterpret_cast<unsigned char*>(d);
    for(int i = 0; i < 32; i++){
        c_digest[i] = (unsigned char)d[i];
    }
    c_digest[32] = 0;

//    unsigned char c_digest[] = "c7fbca202a95a570285e3d700eb04ca2";
    int digest_length = sizeof(c_digest); // 32; // strlen((char*)c_digest) 
    //print_it("    digest: ", c_digest, digest_length);
    //printf("     d len: %d  \n", digest_length);

    EC_KEY *eckey = NULL;
    eckey = EC_KEY_new_by_curve_name(NID_secp256k1);

    BIGNUM *res = new BIGNUM();
    BN_hex2bn(&res, (const char *) std::string(privateKey).c_str());
    EC_KEY_set_private_key(eckey, res);

    ECDSA_SIG *e_signature = ECDSA_do_sign( (unsigned char *) c_digest, digest_length, eckey);
    if (NULL == e_signature)
    {
        printf("Failed to generate EC Signature\n");
        //function_status = -1;
        //return 0;
    } else {
        //printf(" GENERATED SIG \n ");
        //printf("    (sig->r, sig->s): (%s, %s)\n", BN_bn2hex(e_signature->r), BN_bn2hex(e_signature->s));
  
        char *r = BN_bn2hex(e_signature->r);
        char *s = BN_bn2hex(e_signature->s);  
        char * csig = (char *)malloc(129);
        for(int i = 0; i < 64; i++){
            csig[i] = r[i];
        }
        for(int i = 0; i < 64; i++){
            csig[i + 64] =  s[i];
        }
        csig[128] = 0;
        signature = std::string(csig);
        free(csig);
        OPENSSL_free(r);
        OPENSSL_free(s);
    }
    return 1;
} 


/**
* VerifyMessage
*
* Description: Verify a message and signature given a public key.
*
* @param: std::string message. Any text content of any length and mush be the same as was signed.
* @param: std::string signature. hex encoded 128 character signature from SignMessage.
* @param: std::string publicKey. public key that is paird with a private key used to sign the same message.
*/
int CECDSACrypto::VerifyMessage(std::string & message, std::string & signature, std::string & publicKey)
{
    //printf("VerifyMessage \n");    
    //printf("    Message: %s  \n", std::string(message).c_str() );
    //printf("    Public : %s  \n", std::string(publicKey).c_str() ); 
    //printf("    Signat : %s  \n", std::string(signature).c_str() );
//    unsigned char c_digest[] = "c7fbca202a95a570285e3d700eb04ca2";
    unsigned char c_digest[33];
    char d[33];
    sha256((char *)message.c_str(), (char*)d);
    for(int i = 0; i < 32; i++){
        c_digest[i] = (unsigned char)d[i];
    }
    c_digest[32] = 0;
    int digest_length = sizeof(c_digest); // 32; // strlen((char*)c_digest)
    //print_it("    digest: ", c_digest, digest_length);

    BN_CTX *ctx;
    ctx = BN_CTX_new(); // ctx is an optional buffer to save time from allocating and deallocating memory whenever required

    EC_KEY *eckey = NULL;
    eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_POINT *pub_key = NULL;
    pub_key = EC_POINT_hex2point(EC_KEY_get0_group(eckey), std::string(publicKey).c_str(), pub_key, ctx);
    // Verify pub_key ... 
    char *verify_point = EC_POINT_point2hex(EC_KEY_get0_group(eckey), pub_key, (point_conversion_form_t)4, ctx);
    //printf("    verify public point: %s  \n", verify_point);   
 
    int rs = EC_KEY_set_public_key(eckey, pub_key);    

    std::string sr = signature.substr(0, 64);
    std::string ss = signature.substr(64, 64);
    //std::cout << " beep " << sr << " " << ss << std::endl;
    //printf(" Verify %s  %s   \n", r, s);

    ECDSA_SIG *e_signature = ECDSA_SIG_new();
    int len = BN_hex2bn(&e_signature->r, (const char *) std::string(sr).c_str() );
    len = BN_hex2bn(&e_signature->s, (const char *) std::string(ss).c_str() );
    //printf("    Verify (sig->r, sig->s): (%s, %s)\n", BN_bn2hex(e_signature->r), BN_bn2hex(e_signature->s)); 

    //printf("    len: %d \n", digest_length );
    int verify_status = ECDSA_do_verify(c_digest, digest_length, e_signature, eckey);
    const int verify_success = 1;
    if (verify_success != verify_status)
    {
        printf("Failed to verify EC Signature !!! \n");
        return 0;
    }
    else
    {
        printf("Verifed EC Signature \n");
    }

    return 1;
}


/**
* GetKeyPair
* 
* Description: Generate a new key pair.
*
* @param: std::string resulting privateKey
* @param: std::string resulting publicKey
*/
int CECDSACrypto::GetKeyPair(std::string & privateKey, std::string & publicKey) 
{
    //OpenSSL_add_all_algorithms();

    RandomPrivateKey(privateKey);

    GetPublicKey( privateKey, publicKey);

/*
    std::string randomKey;
    RandomPrivateKey(randomKey);
    printf("Random Key: %s\n\n", (char *)randomKey.c_str());
   
    std::string input = "jon";
    char privateHex[65];
    sha256((char *)input.c_str(), privateHex);
    printf(" private (jon) %s \n", privateHex);
    
    //unsigned char * un = usha256((char *)input.c_str());
    //printf(" p unsigned: %s \n", un);

    std::string publicKey2 = "";
    std::string privateKey2(privateHex);
    GetPublicKey( privateKey2, publicKey2);
    
    std::cout << " public key: " << publicKey2 << std::endl;
 
    std::string signature2 = "";
    std::string message = "123";
    SignMessage(privateKey2, message, signature2);
    std::cout << "Signature: " << signature2 << std::endl;
 

    VerifyMessage(message, signature2, publicKey2);

   
    //unsigned char hash[] = "c7fbca202a95a570285e3d700eb04ca2";
    //unsigned char * c_hash = usha256((char *)message.c_str());
    unsigned char hash[32];
    sha256((char *)message.c_str(), (char*)hash); 
    printf("   *** %s \n", hash);
*/
 
    return 1;
}



void CECDSACrypto::sha256(char *string, char outputBuffer[65])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, string, strlen(string));
    SHA256_Final(hash, &sha256);
    int i = 0;
    for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

unsigned char * CECDSACrypto::usha256(char *string)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, string, strlen(string));
    SHA256_Final(hash, &sha256);
    
    return hash;
}

/*
std::string CECDSACrypto::base58_encode(BIGNUM num, std::string vers)
{
    std::string alphabet[58] = {"1","2","3","4","5","6","7","8","9","A","B","C","D","E","F",
    "G","H","J","K","L","M","N","P","Q","R","S","T","U","V","W","X","Y","Z","a","b","c",
    "d","e","f","g","h","i","j","k","m","n","o","p","q","r","s","t","u","v","w","x","y","z"};
    int base_count = 58; 
    std::string encoded; 
    BIGNUM * div = new BIGNUM(); 
    BIGNUM * mod = new BIGNUM();
    while (num >= base_count)
    {
        div = num / base_count;   
        mod = (num - (base_count * div));
        encoded = alphabet[ mod.ConvertToLong() ] + encoded;
	num = div;
    }
    encoded = vers + alphabet[ num.ConvertToLong() ] + encoded;
    return encoded;
}
*/


/**
* runUnitTests
*
* Description: TODO put this in an interface for all classes. 
*/
void CECDSACrypto::runUnitTests()
{
    printf("    Running ECDSA tests. \n");
    // jon  bb472edb86809a761936d90c70aeb4346618aa71da7a00c16e334863499108fd   

    std::string randomKey;
    RandomPrivateKey(randomKey);
    printf("Random Key: %s\n\n", (char *)randomKey.c_str());
  
    std::string input = "jon";
    char privateHex[65];
    sha256((char *)input.c_str(), privateHex);
    printf(" private (jon) %s \n", privateHex);
  
    //unsigned char * un = usha256((char *)input.c_str());
    //printf(" p unsigned: %s \n", un);

    std::string publicKey2 = "";
    std::string privateKey2(privateHex);
    GetPublicKey( privateKey2, publicKey2);

    std::cout << " public key: " << publicKey2 << std::endl;

    std::string signature2 = "";
    std::string message = "123";
    SignMessage(privateKey2, message, signature2);
    std::cout << "Signature: " << signature2 << std::endl;


    int r = VerifyMessage(message, signature2, publicKey2);
    if(r){
        printf(ANSI_COLOR_BLUE "    [PASS]" ANSI_COLOR_RESET  " \n");
    } else {
        printf(ANSI_COLOR_MAGENTA "    [FAIL] " ANSI_COLOR_RESET  " \n");
    }
} 

void CECDSACrypto::print_it(const char* label, const byte* buff, size_t len)
{
    if(!buff || !len)
        return;

    if(label)
        printf("%s: ", label);

    for(size_t i=0; i < len; ++i)
        printf("%02X", buff[i]);

    printf("\n");
}
