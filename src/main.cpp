
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include "crypto/common.h"
#include "crypto/sha256.h"
#include "crypto/hmac_sha256.h"
#include "utilstrencodings.h"

//#include "wallet/wallet.h"
#include "key.h"
//#include "pubkey.h"

//#include <openssl/crypto.h> // no worky
//#include <openssl/ec.h>
#include <stdio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include "rsacrypto.h"

static const uint64_t BUFFER_SIZE = 1000*1000; // Temp


bool generate_key()
{
    int             ret = 0;
    RSA             *r = NULL;
    BIGNUM          *bne = NULL;
    BIO             *bp_public = NULL, *bp_private = NULL;
 
    int             bits = 256;
    unsigned long   e = RSA_F4;
 
    // 1. generate rsa key
    bne = BN_new();
    ret = BN_set_word(bne,e);
    if(ret != 1){
        goto free_all;
    }
 
    r = RSA_new();
    ret = RSA_generate_key_ex(r, bits, bne, NULL);
    if(ret != 1){
        goto free_all;
    }
 
    // 2. save public key
    bp_public = BIO_new_file("public.pem", "w+");
    ret = PEM_write_bio_RSAPublicKey(bp_public, r);
    if(ret != 1){
        goto free_all;
    }
 
    // 3. save private key
    bp_private = BIO_new_file("private.pem", "w+");
    ret = PEM_write_bio_RSAPrivateKey(bp_private, r, NULL, NULL, 0, NULL, NULL);
 
    // 4. free
free_all:
 
    BIO_free_all(bp_public);
    BIO_free_all(bp_private);
    RSA_free(r);
    BN_free(bne);
 
    return (ret == 1);
}



int main()
{
	std::cout << "Magnite Digital Currency v0.0.1" << std::endl;

	// sha256 Test
	CSHA256 sha;
	uint8_t hash[CSHA256::OUTPUT_SIZE];
	std::vector<uint8_t> in(BUFFER_SIZE, 0);
	//while (state.KeepRunning())
        //CSHA256().Write(begin_ptr(in), in.size()).Finalize(hash);
	unsigned char * data = new unsigned char[1024]();
	data[0] = 'H'; data[1] = '1'; data[2] = 0; 
	
	CSHA256().Write(data, 2).Finalize(hash);		
	//std::cout << " hash " << hash << std::endl;
	CSHA256().Finalize(hash);
	//std::cout << " hash " << hash << std::endl;
	std::string message("Test");
	delete[] data;

	// hmac_sha256
	unsigned char * key = new unsigned char[512]();
	key[0] = 'H'; key[1] = '1'; key[2] = 0;
	//CHMAC_SHA256 hmac_sha(key, 2);
	//CHMAC_SHA256().Finalize();

	std::string strUser = "jon";	
	std::string strPass = "123";	
	std::string strSalt = "   ";
	std::string strHash = "What";

	unsigned int KEY_SIZE = 32;
	unsigned char *out = new unsigned char[KEY_SIZE]; 
            
	CHMAC_SHA256(
			reinterpret_cast<const unsigned char*>(strSalt.c_str()), 
			strSalt.size()
		    ).Write(reinterpret_cast<const unsigned char*>(strPass.c_str()), strPass.size()).Finalize(out);
	std::vector<unsigned char> hexvec(out, out+KEY_SIZE);
	std::string strHashFromPass = HexStr(hexvec);
	//std::cout << " xxx " << strHashFromPass  << std::endl;


	unsigned char *out2 = new unsigned char[KEY_SIZE];	
	CHMAC_SHA256(
			reinterpret_cast<const unsigned char*>(strSalt.c_str()),
                        strSalt.size()
                    ).Write(reinterpret_cast<const unsigned char*>(strUser.c_str()), strUser.size()).Finalize(out2);
        std::vector<unsigned char> hexvec2(out2, out2+KEY_SIZE);
        std::string strHashFromPass2 = HexStr(hexvec2);
        //std::cout << " 2 " << strHashFromPass2  << std::endl;	


	unsigned char *out3 = new unsigned char[KEY_SIZE];
	//CSHA256().Write(nonce.begin(), 32).Write(hash.begin(), 32).Write(&pubkey[0], pubkey.size()).Write(&vchSig[0], vchSig.size()).Finalize(entry.begin());
	CSHA256().Write(reinterpret_cast<const unsigned char*>(strUser.c_str()), strUser.size()).Finalize(out3);
	std::vector<unsigned char> hexvec3(out3, out3+KEY_SIZE);
        std::string strHashFromPass3 = HexStr(hexvec3);
        std::cout << " sha256 of string " << strUser << " " << strHashFromPass3  << std::endl;


	// sha256 123 = a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3 
	// sah256 jon = bb472edb86809a761936d90c70aeb4346618aa71da7a00c16e334863499108fd

	CKey secret; // 
	//secret.resize(32);
        secret.MakeNewKey(true);
	std::cout << " Secret key... " << std::endl;		

	//CPrivKey x = secret.GetPrivKey(); // Seg fault
	std::cout << " Private key... " << std::endl;

	//vector<unsigned char> vchPubKey = secret.GetPubKey(); 
	//CPubKey pubkey = secret.GetPubKey();  // seg fault
	//assert(secret.VerifyPubKey(pubkey));

	//generate_key();

	//ECC_InitSanityCheck(); // seg fault

	CRSACrypto crypto;
	std::string p;
	std::string v;
	crypto.GetKeyPair(p,v);
	std::cout << " public: " << p << "  private: " << v << std::endl;
	//std::string message = "Do eeet.";
	std::string signature = "";
        crypto.SignMessage(p, message, signature);
	std::cout << " sig " << signature << std::endl;	


	//byte * back = NULL;
        //int len = 0;
        //crypto.StringToData(signature, back, &len);
	//std::cout << " back "  <<  " len " << len << std::endl;
	//std::string again = "";
	//crypto.DataToString(back, len, again);
        //std::cout << " again " << again << std::endl;	


	int verified = crypto.verify(message, signature, v);
	std::cout << " verify " << verified << std::endl;

	// pubkey.GetID()
	std::cout << " Done " << std::endl;
}
