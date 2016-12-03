// Copyright (c) 2016 Jon Taylor 
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ecdsacrypto.h"


//int CECDSACrypto::RandomPrivateKey()


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

    //int i;
    //for (i=0; i<130; i++) // 1 byte 0x42, 32 bytes for X coordinate, 32 bytes for Y coordinate
    //{
    //   printf("%c", *c++);
    //}
    //printf("\n");

    publicKey = std::string(cc);
     
    BN_CTX_free(ctx);
    free(cc);
    return 1;
}


int CECDSACrypto::SignMessage(std::string & privateKey, std::string & message, std::string & signature)
{
    unsigned char * c_digest = usha256((char *)message.c_str());
    //unsigned char c_digest[] = "c7fbca202a95a570285e3d700eb04ca2";

    EC_KEY *eckey = NULL;
    eckey = EC_KEY_new_by_curve_name(NID_secp256k1);

    BIGNUM *res;
    BN_hex2bn(&res, privateKey.c_str());

    EC_KEY_set_private_key(eckey, res);

    ECDSA_SIG *e_signature = ECDSA_do_sign( (unsigned char *) c_digest , strlen((char*)c_digest), eckey);
    if (NULL == e_signature)
    {
        printf("Failed to generate EC Signature\n");
        //function_status = -1;
    } else {
        printf(" GENERATED SIG \n ");

        printf("(sig->r, sig->s): (%s, %s)\n", BN_bn2hex(e_signature->r), BN_bn2hex(e_signature->s));
        printf("(sig->r, sig->s): (%s, %s)\n", BN_bn2hex(e_signature->r), BN_bn2hex(e_signature->s)); 
    
        // Get der encoded signature
        const uint8_t *der_copy;
        int der_len = ECDSA_size(eckey);
        const uint8_t * der = (const uint8_t *)calloc(der_len, sizeof(uint8_t));
        der_copy = der;
        i2d_ECDSA_SIG( (const ECDSA_SIG *) e_signature, (unsigned char **) * der_copy);

        printf(" DER %d  \n", der_copy );
    }

    return 1;
} 

int CECDSACrypto::VerifyMessage(std::string & message, std::string & signature, std::string & publicKey)
{
    unsigned char hash[] = "c7fbca202a95a570285e3d700eb04ca2";

    EC_KEY *eckey = NULL;
    eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_POINT * ec_point;

    int r = EC_KEY_set_public_key(eckey, ec_point);    

/*
    int verify_status = ECDSA_do_verify(hash, strlen((char*)hash), signature, eckey);
    const int verify_success = 1;
    if (verify_success != verify_status)
                        {
                            printf("Failed to verify EC Signature\n");
                            //function_status = -1;
                        }
                        else
                        {
                            printf("Verifed EC Signature\n");
                            //function_status = 1;
                        }
*/
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
    OpenSSL_add_all_algorithms();

    unsigned char hash[] = "c7fbca202a95a570285e3d700eb04ca2";


    std::string input = "jon";
    char privateHex[65];
    sha256((char *)input.c_str(), privateHex);
    printf(" private %s \n", privateHex);
    
    unsigned char * un = usha256((char *)input.c_str());
    printf(" p unsigned: %s \n", un);

    std::string publicKey2 = "";
    std::string privateKey2(privateHex);
    GetPublicKey( privateKey2, publicKey2);
    
    std::cout << " yo " << publicKey2 << std::endl;
 
    std::string signature2 = "";
    std::string message = "123";
    SignMessage(privateKey2, message, signature2);
    std::cout << " sig " << signature2 << std::endl;

    
    /* Sign and Verify HMAC keys */
    EVP_PKEY *skey = NULL, *vkey = NULL;

    BIGNUM start;
    BN_init(&start);

    BIGNUM *res;
    res = &start;
    //BN_hex2bn(&res,"18E14A7B6A307F426A94F8114701E7C8E774E7F9A47E2C2035DB29A206321725");
    BN_hex2bn(&res, privateHex);

    BN_CTX *ctx;
    ctx = BN_CTX_new(); // ctx is an optional buffer to save time from allocating and deallocating memory whenever required

    int function_status = -1;
    EC_KEY *eckey=EC_KEY_new();
    
    EC_POINT *pub_key = NULL;

    if (NULL == eckey)
    {
        printf("Failed to create new EC Key\n");
        function_status = -1;
    }
    else
    {
        // NID_secp192k1  NID_secp256k1   
        EC_GROUP *ecgroup = EC_GROUP_new_by_curve_name(NID_secp256k1);
        if (NULL == ecgroup)
        {
            printf("Failed to create new EC Group\n");
            function_status = -1;
        }
        else
        {
            int set_group_status = EC_KEY_set_group(eckey, ecgroup);
            const int set_group_success = 1;
            if (set_group_success != set_group_status)
            {
                printf("Failed to set group for EC Key\n");
                function_status = -1;
            }
            else
            {
                const int gen_success = 1;
                int gen_status = EC_KEY_generate_key(eckey);
                if (gen_success != gen_status)
                {
                    printf("Failed to generate EC Key\n");
                    function_status = -1;
                }
                else
                {

		    pub_key = EC_POINT_new(ecgroup);

		    /* pub_key is a new uninitialized `EC_POINT*`.  priv_key res is a `BIGNUM*`. */
		    if (!EC_POINT_mul(ecgroup, pub_key, res, NULL, NULL, ctx))
			       printf("Error at EC_POINT_mul.\n");
			
		    char *cc = EC_POINT_point2hex(ecgroup, pub_key, (point_conversion_form_t)4, ctx);			
		    printf(" public %s  \n", cc);

		    //char *pp = 




                    ECDSA_SIG *signature = ECDSA_do_sign(hash, strlen((char*)hash), eckey);
                    if (NULL == signature)
                    {
                        printf("Failed to generate EC Signature\n");
                        function_status = -1;
                    }
                    else
                    {
			// Print signature? 

                        int verify_status = ECDSA_do_verify(hash, strlen((char*)hash), signature, eckey);
                        const int verify_success = 1;
                        if (verify_success != verify_status)
                        {
                            printf("Failed to verify EC Signature\n");
                            function_status = -1;
                        }
                        else
                        {
                            printf("Verifed EC Signature\n");
                            function_status = 1;
                        }
                    }

		    free(cc);
                }
            }
            EC_GROUP_free(ecgroup);
        }
        EC_KEY_free(eckey);
    }

    BN_CTX_free(ctx);

    return function_status;
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

