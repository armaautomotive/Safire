// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hex.h"

#include <sstream>
#include <unistd.h>   // open and close
#include <sys/stat.h> // temp because we removed util
#include <fcntl.h> // temp removed util.h
#include <time.h>

/**      
* RandomPrivateKey
*        
* Description: private key.
*     Generates 128 random ascii characters to feed into a sha256 hash.
*            
* @param: std::string output private key.
* @return int returns 1 is successfull.
*/  
std::string CHex::to64(std::string & input){
	std::string result = "";
	for( int i = 0; i < input.length; i++ ){

		

	}
	return result;
}



/*
import com.dynamicflash.util.Base64; // !use this Base64 class!
 
public static function p16Top64(p16:String):String
{
    var b:ByteArray = new ByteArray();
    var s:String = p16;
    while(s.length)
    {
        b.writeUnsignedInt(parseInt("0x" + s.substr(0, 8), 16));
        s = s.substr(8);
    }
    b.position = 0;
    return Base64.encodeByteArray(b);
}
 
public static function p64Top16(p64:String):String
{
    var r:String = "";
    var b:ByteArray = Base64.decodeToByteArray(p64);
    var x:String = "";
    b.position = 0;
    while(b.position < b.length)
    {
        x = b.readUnsignedInt().toString(16);
        r += String("00000000").substr(x.length) + x;
    }
    return r;
}
*/
