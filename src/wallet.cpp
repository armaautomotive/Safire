// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"

#include <fstream>      // std::ifstream
#include <sstream>
#include <unistd.h>   // open and close
#include <string>
#include <iostream>


// TODO: Add function that checks and creates new wallet

bool CWallet::fileExists(std::string fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

bool CWallet::write(std::string privateKey, std::string publicKey)
{
    std::string walletContent = "private:" + privateKey + "\n" +
            "public:" + publicKey + "\n";

    //const char *homeDir = getenv("HOME");
    //std::cout << " dir " << homeDir << "\n";

    std::ofstream out("wallet.dat");
    out << walletContent;
    out.close();

    return true;
}

bool CWallet::read(std::string & privateKey, std::string & publicKey)
{
    //const char *homeDir = getenv("HOME");
    //std::cout << " dir " << homeDir << "\n";

    std::ifstream t("wallet.dat");
    std::string walletContent((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());

    //std::cout << " file: " << walletContent << std::endl;
    std::size_t privateStart = walletContent.find("private:", 0);  
    std::size_t privateEnd   = walletContent.find("\n", privateStart);
    std::size_t publicStart  = walletContent.find("public:", 0);
    std::size_t publicEnd    = walletContent.find("\n", publicStart);
    if (privateStart!=std::string::npos) {
        privateKey = walletContent.substr(privateStart + 8 , privateEnd - privateStart - 8);
        publicKey = walletContent.substr(publicStart + 7 , publicEnd - publicStart - 7);
    }
    return true;
}

