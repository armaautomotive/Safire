// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"

#include <fstream>      // std::ifstream
#include <sstream>
#include <unistd.h>   // open and close
#include <string>
#include <iostream>

bool CWallet::fileExists(std::string fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

bool CWallet::write(std::string privateKey, std::string publicKey)
{
    std::string walletContent = "private:" + privateKey + "\n" +
            "public:" + publicKey + "\n";

    std::ofstream out("wallet.dat");
    out << walletContent;
    out.close();

    return true;
}

bool CWallet::read(std::string & privateKey, std::string & publicKey)
{
    std::ifstream t("wallet.dat");
    std::string walletContent((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());

    std::cout << " file: " << walletContent << std::endl;

    return true;
}

