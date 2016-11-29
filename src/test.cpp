// Mac see: http://stackoverflow.com/questions/34178988/openssl-ssl-h-not-found-but-installed-with-homebrew
// clang++ -L/usr/local/opt/openssl/lib -I/usr/local/opt/openssl/include test.cpp -v -o test

#include <stdio.h>
#include <stdlib.h>
//#include <openssl/ssl.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>

int main()
{
     EC_KEY *eckey = NULL;
     EC_POINT *pub_key = NULL;
     const EC_GROUP *group = NULL;
     BIGNUM start;
     BIGNUM *res;
     BN_CTX *ctx;

     BN_init(&start);
     //ctx = BN_CTX_new(); // ctx is an optional buffer to save time from allocating and deallocating memory whenever required

     res = &start;
     //BN_hex2bn(&res,"30caae2fcb7c34ecadfddc45e0a27e9103bd7cfc87730d7818cc096b1266a683");
     //eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
     //group = EC_KEY_get0_group(eckey);
     //pub_key = EC_POINT_new(group);

     //EC_KEY_set_private_key(eckey, res);

     /* pub_key is a new uninitialized `EC_POINT*`.  priv_key res is a `BIGNUM*`. */
     //if (!EC_POINT_mul(group, pub_key, res, NULL, NULL, ctx))
     //  printf("Error at EC_POINT_mul.\n");

     //EC_KEY_set_public_key(eckey, pub_key);


	/*
     char *cc = EC_POINT_point2hex(group, pub_key, 4, ctx);

     char *c=cc;

     int i;

     for (i=0; i<130; i++) // 1 byte 0x42, 32 bytes for X coordinate, 32 bytes for Y coordinate
     {
       printf("%c", *c++);
     }

     printf("\n");

     BN_CTX_free(ctx);

     free(cc);
	*/

     return 0;
}

