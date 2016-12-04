// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transaction.h"

#include <fstream>      // std::ifstream
#include <sstream>
#include <unistd.h>     // open and close
#include <string>
#include <iostream>
#include "ecdsacrypto.h"      // sha256

/**
* joinNetwork
*
* Description: Compose a request to join the network message.
*/
std::string CTransaction::joinNetwork(std::string publicKey)
{
    std::string message("{action='join',publicKey='");
    message += publicKey;
    message += "'}";
    return message;
}


std::string CTransaction::sendPayment(std::string privateKey, std::string publicKey, std::string toAddress, double amount, int id)
{
    std::string amountAsString = std::to_string(amount);
    std::string idAsString = std::to_string(id);
    std::string messageContent(publicKey + toAddress + amountAsString + idAsString);
    CECDSACrypto crypto;
    char d[65];
    crypto.sha256((char *)messageContent.c_str(), (char*)d);
    d[64] = 0;
    std::string messageDigest(d);
    std::string signature("");
    crypto.SignMessage(privateKey, messageDigest, signature);
    std::string message("{action='payment',\n    from='");
    message += publicKey;
    message += "',\n    to='" + toAddress + "',\n    amount="+amountAsString+",\n    id="+idAsString+",\n    signature='"+signature+"'}";
    return message;
}

bool CTransaction::verifyPayment(std::string message)
{

    return false;
} 

