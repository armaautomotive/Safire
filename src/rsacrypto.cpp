// Copyright (c) 2016 Jon Taylor 
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rsacrypto.h"


/**
* GetKeyPair
* 
* Description: Generate a new key pair.
*
* @param: std::string resulting privateKey
* @param: std::string resulting publicKey
*/
int CRSACrypto::GetKeyPair(std::string & privateKey, std::string & publicKey) 
{
    OpenSSL_add_all_algorithms();
    /* Sign and Verify HMAC keys */
    EVP_PKEY *skey = NULL, *vkey = NULL;

    int rc = make_keys(&skey, &vkey);
    assert(rc == 0);
    if(rc != 0)
        exit(1);

    assert(skey != NULL);
    if(skey == NULL)
        exit(1);

    assert(vkey != NULL);
    if(vkey == NULL)
        exit(1);

    RSA * rsa_p = EVP_PKEY_get1_RSA(vkey);
    RSA * rsa_s = EVP_PKEY_get1_RSA(skey);
    BIO *pri = BIO_new(BIO_s_mem());
    BIO *pub = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPrivateKey(pri, rsa_s, NULL, NULL, 0, NULL, NULL);
    PEM_write_bio_RSAPublicKey(pub, rsa_p);
    int pri_len = BIO_pending(pri);
    int pub_len = BIO_pending(pub);
    char * pri_key = (char*)malloc(pri_len + 1);
    char * pub_key = (char*)malloc(pub_len + 1);
    BIO_read(pri, pri_key, pri_len);
    BIO_read(pub, pub_key, pub_len);
    pub_key[pub_len] = '\0';
    pri_key[pri_len] = '\0';
    //printf("\n %s \n", pub_key);
    //printf("\n %s \n", pri_key);

    privateKey = std::string(pri_key);
    publicKey = std::string(pub_key);

    free(pri_key);
    free(pub_key);

    const byte msg[] = "Ur a lil bitch. LOL.";
    byte* sig = NULL;
    size_t slen = 0;

    /* Using the skey or signing key */
    rc = sign_it(msg, sizeof(msg), &sig, &slen, skey);
    assert(rc == 0);
    if(rc == 0) {
        printf("Created signature\n");
    } else {
        printf("Failed to create signature, return code %d\n", rc);
        exit(1); /* Should cleanup here */
    }

    print_it("Signature", sig, slen);

#if 0
    /* Tamper with signature */
    printf("Tampering with signature\n");
    sig[0] ^= 0x01;
#endif

#if 0
    /* Tamper with signature */
    printf("Tampering with signature\n");
    sig[slen - 1] ^= 0x01;
#endif

    /* Using the vkey or verifying key */
    rc = verify_it(msg, sizeof(msg), sig, slen, vkey);
    if(rc == 0) {
        printf("Verified signature\n");
    } else {
        printf("Failed to verify signature, return code %d\n", rc);
    }


/*
    // PRIVATE
    {
    int pkeyLen;
    unsigned char *ucBuf, *uctempBuf;
    pkeyLen = i2d_PublicKey(skey, NULL);
    ucBuf = (unsigned char *)malloc(pkeyLen+1);
    uctempBuf = ucBuf;
    i2d_PublicKey(skey, &uctempBuf);
    
    int ii;
    printf("Private key: ");
    for (ii = 0; ii < pkeyLen; ii++)
    {
        printf("%02x", (unsigned char) ucBuf[ii]);
    }
    printf("\n");
    }

    // PUBLIC
    {
    int pkeyLen2;
    unsigned char *ucBuf2, *uctempBuf2;
    pkeyLen2 = i2d_PublicKey(vkey, NULL);
    ucBuf2 = (unsigned char *)malloc(pkeyLen2+1);
    uctempBuf2 = ucBuf2;
    i2d_PublicKey(vkey, &uctempBuf2);
    printf("Public key:  ");
    int ii;
    for (ii = 0; ii < pkeyLen2; ii++)
    {
        printf("%02x", (unsigned char) ucBuf2[ii]);
    }
    printf("\n");
    }
*/

    return 1;
}


int CRSACrypto::SignMessage(std::string & privateKey, std::string & message, std::string & signature)
{
	// 

	EVP_PKEY *skey = NULL;
	skey = EVP_PKEY_new();

	char private_key[4096];
	strncpy(private_key, privateKey.c_str(), sizeof(private_key));
	private_key[sizeof(private_key) - 1] = 0;

	BIO *bio = BIO_new_mem_buf((void*)private_key, (int)strlen(private_key));
	RSA *rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, 0, NULL);
	BIO_free(bio);


	/* Set signing key */
        int rc = EVP_PKEY_assign_RSA(skey, RSAPrivateKey_dup(rsa));
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_PKEY_assign_RSA (1) failed, error 0x%lx\n", ERR_get_error());
            //break; /* failed */
        }

	byte* sig = NULL;
	size_t slen = 0;

	/* Using the skey or signing key */
	
	const byte * msg = (const byte*)message.c_str();

	rc = sign_it(msg, sizeof(msg), &sig, &slen, skey);
	assert(rc == 0);
	if(rc == 0) {
		printf("Created signature\n");
	} else {
		printf("Failed to create signature, return code %d\n", rc);
		exit(1); /* Should cleanup here */
	}


	print_it("Signature", sig, slen);

	//std::string signature = "";
	DataToString(sig, slen, signature);
	//printf("Signature!!!: %s ", signature);
	//std::cout << "Signature!!! " << signature << std::endl;

	//byte * back = NULL;
	//int len = 0;
	//StringToData(signature, back, &len);
	//print_it("Check : ", back, len); 


	//signature = std::string(sig);
	return 1;
}


int CRSACrypto::make_keys(EVP_PKEY** skey, EVP_PKEY** vkey)
{
    int result = -1;

    if(!skey || !vkey)
        return -1;

    if(*skey != NULL) {
        EVP_PKEY_free(*skey);
        *skey = NULL;
    }

    if(*vkey != NULL) {
        EVP_PKEY_free(*vkey);
        *vkey = NULL;
    }

    RSA* rsa = NULL;

    do
    {
        *skey = EVP_PKEY_new();
        assert(*skey != NULL);
        if(*skey == NULL) {
            printf("EVP_PKEY_new failed (1), error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        *vkey = EVP_PKEY_new();
        assert(*vkey != NULL);
        if(*vkey == NULL) {
            printf("EVP_PKEY_new failed (2), error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        // 2048
        // 1024
        // 512
        // 384   error
        // 256   error
        //   RSA_F4  RSA_3
        rsa = RSA_generate_key(512, RSA_F4, NULL, NULL);
        assert(rsa != NULL);
        if(rsa == NULL) {
            printf("RSA_generate_key failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }
	/* Set signing key */
        int rc = EVP_PKEY_assign_RSA(*skey, RSAPrivateKey_dup(rsa));
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_PKEY_assign_RSA (1) failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        /* Sanity check. Verify private exponent is present */
        // assert(EVP_PKEY_get0_RSA(*skey)->d != NULL);

        /* Set verifier key */
        rc = EVP_PKEY_assign_RSA(*vkey, RSAPublicKey_dup(rsa));
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_PKEY_assign_RSA (2) failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        /* Sanity check. Verify private exponent is missing */
        /* assert(EVP_PKEY_get0_RSA(*vkey)->d == NULL); */

        result = 0;

    } while(0);

    if(rsa) {
        RSA_free(rsa);
        rsa = NULL;
    }

    return !!result;
}

int CRSACrypto::sign_it(const byte* msg, size_t mlen, byte** sig, size_t* slen, EVP_PKEY* pkey)
{
    /* Returned to caller */
    int result = -1;

    if(!msg || !mlen || !sig || !pkey) {
        assert(0);
        return -1;
    }
   
    if(*sig)
        OPENSSL_free(*sig);

    *sig = NULL;
    *slen = 0;

    EVP_MD_CTX* ctx = NULL;

    do
    {
        ctx = EVP_MD_CTX_create();
        assert(ctx != NULL);
        if(ctx == NULL) {
            printf("EVP_MD_CTX_create failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        const EVP_MD* md = EVP_get_digestbyname(hn);
        assert(md != NULL);
        if(md == NULL) {
            printf("EVP_get_digestbyname failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        int rc = EVP_DigestInit_ex(ctx, md, NULL);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestInit_ex failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        rc = EVP_DigestSignInit(ctx, NULL, md, NULL, pkey);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestSignInit failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }
	rc = EVP_DigestSignUpdate(ctx, msg, mlen);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestSignUpdate failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        size_t req = 0;
        rc = EVP_DigestSignFinal(ctx, NULL, &req);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestSignFinal failed (1), error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        assert(req > 0);
        if(!(req > 0)) {
            printf("EVP_DigestSignFinal failed (2), error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        //*sig = OPENSSL_malloc(req);
        *sig = (byte*)OPENSSL_malloc(req);
        assert(*sig != NULL);
        if(*sig == NULL) {
            printf("OPENSSL_malloc failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        *slen = req;
        rc = EVP_DigestSignFinal(ctx, *sig, slen);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestSignFinal failed (3), return code %d, error 0x%lx\n", rc, ERR_get_error());
            break; /* failed */
        }

        assert(req == *slen);
        if(rc != 1) {
            printf("EVP_DigestSignFinal failed, mismatched signature sizes %ld, %ld", req, *slen);
            break; /* failed */
        }

        result = 0;

    } while(0);

    if(ctx) {
        EVP_MD_CTX_destroy(ctx);
        ctx = NULL;
    }
    return !!result;
}


int CRSACrypto::verify(std::string message, std::string sig, std::string pkey)
{
	//EVP_PKEY k = NULL;		

	//verify_it( (const byte*)message.c_str(), message.length(), (const byte*)sig, sig.length(), k );
	return 1;
}

int CRSACrypto::verify_it(const byte* msg, size_t mlen, const byte* sig, size_t slen, EVP_PKEY* pkey)
{
    /* Returned to caller */
    int result = -1;

    if(!msg || !mlen || !sig || !slen || !pkey) {
        assert(0);
        return -1;
    }
   
    EVP_MD_CTX* ctx = NULL;

    do
    {
        ctx = EVP_MD_CTX_create();
        assert(ctx != NULL);
        if(ctx == NULL) {
            printf("EVP_MD_CTX_create failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        const EVP_MD* md = EVP_get_digestbyname(hn);
        assert(md != NULL);
        if(md == NULL) {
            printf("EVP_get_digestbyname failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        int rc = EVP_DigestInit_ex(ctx, md, NULL);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestInit_ex failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        rc = EVP_DigestVerifyInit(ctx, NULL, md, NULL, pkey);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestVerifyInit failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        rc = EVP_DigestVerifyUpdate(ctx, msg, mlen);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestVerifyUpdate failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }
        /* Clear any errors for the call below */
        ERR_clear_error();

        rc = EVP_DigestVerifyFinal(ctx, sig, slen);
        assert(rc == 1);
        if(rc != 1) {
            printf("EVP_DigestVerifyFinal failed, error 0x%lx\n", ERR_get_error());
            break; /* failed */
        }

        result = 0;

    } while(0);
   
    if(ctx) {
        EVP_MD_CTX_destroy(ctx);
        ctx = NULL;
    }
   
    return !!result;

}

void CRSACrypto::print_it(const char* label, const byte* buff, size_t len)
{
    if(!buff || !len)
        return;

    if(label)
        printf("%s: ", label);

    for(size_t i=0; i < len; ++i)
        printf("%02X", buff[i]);

    printf("\n");
}

void CRSACrypto::DataToString(const byte * buff, size_t buff_len, std::string & str)
{
	if(!buff || !buff_len)
		return;

	for(size_t i=0; i < buff_len; ++i)
	{
		char b[3];
		snprintf(b, sizeof(b), "%02X", buff[i]);
		str += b;
	}
}


void CRSACrypto::StringToData(std::string & str, const byte * buff, int * buff_len)
{
	byte * data = ( byte*)malloc( (str.length()/2) );	
	int length = 0;

	int target = 0;	
	for(unsigned int i = 0; i < str.length(); )
        {	
		std::string s_hex = str.substr(i, 2);
								
		printf(" hex _%s_ ", s_hex.c_str());

		unsigned uchr;
		sscanf( s_hex.c_str(), "%2x", &uchr );
		data[target++] = uchr;	
		printf(" char %u ", uchr);

		i += 2;
		length += 1;
	}

	buff = (const byte*)data;
	*buff_len = length;
}





