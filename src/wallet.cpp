// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"

#include "ecdsacrypto.h"
#include <fstream>      // std::ifstream
#include <sstream>
#include <unistd.h>   // open and close
#include <string>
#include <iostream>
#include <vector>

namespace {

const char* LEGACY_WALLET_FILE = "wallet.dat";
const char* WALLET_STORE_FILE = "wallets.dat";

std::string trim(const std::string& value)
{
    std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string safeLabel(std::string label)
{
    label = trim(label);
    for (int i = 0; i < label.length(); ++i) {
        if (label.at(i) == '\t' || label.at(i) == '\n' || label.at(i) == '\r') {
            label[i] = ' ';
        }
    }
    if (label.length() == 0) {
        return "Account";
    }
    return label;
}

std::vector<std::string> splitTabs(const std::string& line)
{
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, '\t')) {
        parts.push_back(item);
    }
    return parts;
}

bool readLegacyWallet(std::string& privateKey, std::string& publicKey)
{
    std::ifstream t(LEGACY_WALLET_FILE);
    if (!t.good()) {
        return false;
    }
    std::string walletContent((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());

    std::size_t privateStart = walletContent.find("private:", 0);
    std::size_t privateEnd   = walletContent.find("\n", privateStart);
    std::size_t publicStart  = walletContent.find("public:", 0);
    std::size_t publicEnd    = walletContent.find("\n", publicStart);
    if (privateStart == std::string::npos || publicStart == std::string::npos) {
        return false;
    }
    if (privateEnd == std::string::npos) {
        privateEnd = walletContent.length();
    }
    if (publicEnd == std::string::npos) {
        publicEnd = walletContent.length();
    }
    privateKey = walletContent.substr(privateStart + 8 , privateEnd - privateStart - 8);
    publicKey = walletContent.substr(publicStart + 7 , publicEnd - publicStart - 7);
    return privateKey.length() > 0 && publicKey.length() > 0;
}

bool writeLegacyWallet(const std::string& privateKey, const std::string& publicKey)
{
    std::string walletContent = "private:" + privateKey + "\n" +
            "public:" + publicKey + "\n";
    std::ofstream out(LEGACY_WALLET_FILE);
    out << walletContent;
    out.close();
    return true;
}

bool readWalletStore(std::vector<CWallet::account>& accounts, std::string& activeId, std::string& creatorId)
{
    accounts.clear();
    activeId = "";
    creatorId = "";
    std::ifstream infile(WALLET_STORE_FILE);
    if (!infile.good()) {
        return false;
    }

    std::string line;
    while (std::getline(infile, line)) {
        if (line.find("active:") == 0) {
            activeId = trim(line.substr(7));
            continue;
        }
        if (line.find("creator:") == 0) {
            creatorId = trim(line.substr(8));
            continue;
        }
        if (line.find("wallet\t") != 0) {
            continue;
        }
        std::vector<std::string> parts = splitTabs(line);
        if (parts.size() < 5) {
            continue;
        }
        CWallet::account account;
        account.id = parts.at(1);
        account.label = parts.at(2);
        account.private_key = parts.at(3);
        account.public_key = parts.at(4);
        if (account.id.length() == 0) {
            account.id = account.public_key;
        }
        if (account.public_key.length() > 0 && account.private_key.length() > 0) {
            accounts.push_back(account);
        }
    }
    if (activeId.length() == 0 && accounts.size() > 0) {
        activeId = accounts.at(0).id;
    }
    if (creatorId.length() == 0 && accounts.size() > 0) {
        creatorId = accounts.at(0).id;
    }
    return true;
}

bool writeWalletStore(const std::vector<CWallet::account>& accounts, const std::string& activeId, const std::string& creatorId)
{
    std::ofstream out(WALLET_STORE_FILE);
    if (!out.good()) {
        return false;
    }
    out << "active:" << activeId << "\n";
    out << "creator:" << creatorId << "\n";
    for (int i = 0; i < accounts.size(); ++i) {
        CWallet::account account = accounts.at(i);
        out << "wallet\t"
            << account.id << "\t"
            << safeLabel(account.label) << "\t"
            << account.private_key << "\t"
            << account.public_key << "\n";
    }
    out.close();
    return true;
}

} // namespace


// TODO: Add function that checks and creates new wallet

bool CWallet::fileExists(std::string fileName)
{
    if (fileName.compare(LEGACY_WALLET_FILE) == 0) {
        std::ifstream store(WALLET_STORE_FILE);
        if (store.good()) {
            return true;
        }
    }
    std::ifstream infile(fileName);
    return infile.good();
}

bool CWallet::write(std::string privateKey, std::string publicKey)
{
    writeLegacyWallet(privateKey, publicKey);

    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    readWalletStore(accounts, activeId, creatorId);

    bool found = false;
    for (int i = 0; i < accounts.size(); ++i) {
        if (accounts.at(i).public_key.compare(publicKey) == 0) {
            accounts[i].private_key = privateKey;
            accounts[i].id = publicKey;
            found = true;
        }
    }
    if (!found) {
        CWallet::account account;
        account.id = publicKey;
        account.label = accounts.empty() ? "Main" : "Account";
        account.private_key = privateKey;
        account.public_key = publicKey;
        accounts.push_back(account);
    }
    if (activeId.length() == 0) {
        activeId = publicKey;
    }
    if (creatorId.length() == 0) {
        creatorId = accounts.at(0).id;
    }
    return writeWalletStore(accounts, activeId, creatorId);
}

bool CWallet::read(std::string & privateKey, std::string & publicKey)
{
    if (!ensureWalletStore()) {
        return readLegacyWallet(privateKey, publicKey);
    }
    return readAccount(activeAccountId(), privateKey, publicKey);
}

bool CWallet::readCreatorAccount(std::string & privateKey, std::string & publicKey)
{
    if (!ensureWalletStore()) {
        return readLegacyWallet(privateKey, publicKey);
    }
    return readAccount(creatorAccountId(), privateKey, publicKey);
}

bool CWallet::readAccount(std::string id, std::string & privateKey, std::string & publicKey)
{
    ensureWalletStore();
    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    readWalletStore(accounts, activeId, creatorId);
    if (id.length() == 0) {
        id = activeId;
    }
    for (int i = 0; i < accounts.size(); ++i) {
        if (accounts.at(i).id.compare(id) == 0 || accounts.at(i).public_key.compare(id) == 0) {
            privateKey = accounts.at(i).private_key;
            publicKey = accounts.at(i).public_key;
            return true;
        }
    }
    return false;
}

bool CWallet::createAccount(std::string label, CWallet::account & created)
{
    CECDSACrypto ecdsa;
    std::string privateKey;
    std::string publicKey;
    std::string publicKeyUncompressed;
    if (!ecdsa.RandomPrivateKey(privateKey)) {
        return false;
    }
    ecdsa.GetPublicKey(privateKey, publicKeyUncompressed, publicKey);

    ensureWalletStore();
    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    readWalletStore(accounts, activeId, creatorId);

    created.id = publicKey;
    created.label = safeLabel(label);
    created.private_key = privateKey;
    created.public_key = publicKey;
    accounts.push_back(created);
    if (activeId.length() == 0) {
        activeId = created.id;
    }
    if (creatorId.length() == 0) {
        creatorId = accounts.at(0).id;
    }
    return writeWalletStore(accounts, activeId, creatorId);
}

std::vector<CWallet::account> CWallet::listAccounts()
{
    ensureWalletStore();
    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    readWalletStore(accounts, activeId, creatorId);
    return accounts;
}

bool CWallet::setActiveAccount(std::string id)
{
    ensureWalletStore();
    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    readWalletStore(accounts, activeId, creatorId);
    for (int i = 0; i < accounts.size(); ++i) {
        if (accounts.at(i).id.compare(id) == 0 || accounts.at(i).public_key.compare(id) == 0) {
            writeLegacyWallet(accounts.at(i).private_key, accounts.at(i).public_key);
            if (creatorId.length() == 0) {
                creatorId = accounts.at(0).id;
            }
            return writeWalletStore(accounts, accounts.at(i).id, creatorId);
        }
    }
    return false;
}

bool CWallet::setCreatorAccount(std::string id)
{
    ensureWalletStore();
    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    readWalletStore(accounts, activeId, creatorId);
    for (int i = 0; i < accounts.size(); ++i) {
        if (accounts.at(i).id.compare(id) == 0 || accounts.at(i).public_key.compare(id) == 0) {
            return writeWalletStore(accounts, activeId, accounts.at(i).id);
        }
    }
    return false;
}

std::string CWallet::activeAccountId()
{
    ensureWalletStore();
    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    readWalletStore(accounts, activeId, creatorId);
    return activeId;
}

std::string CWallet::creatorAccountId()
{
    ensureWalletStore();
    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    readWalletStore(accounts, activeId, creatorId);
    return creatorId;
}

bool CWallet::ensureWalletStore()
{
    std::vector<CWallet::account> accounts;
    std::string activeId;
    std::string creatorId;
    if (readWalletStore(accounts, activeId, creatorId) && accounts.size() > 0) {
        return true;
    }

    std::string privateKey;
    std::string publicKey;
    if (!readLegacyWallet(privateKey, publicKey)) {
        return false;
    }

    CWallet::account account;
    account.id = publicKey;
    account.label = "Main";
    account.private_key = privateKey;
    account.public_key = publicKey;
    accounts.push_back(account);
    return writeWalletStore(accounts, account.id, account.id);
}
