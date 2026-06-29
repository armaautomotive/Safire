// Copyright (c) 2026 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_CHAINVALIDATOR_H
#define SAFIRE_CHAINVALIDATOR_H

#include "functions/functions.h"
#include <string>

class CBlockDB;

class CChainValidator
{
public:
    static bool validateBlockForStorage(CBlockDB& block_db, const CFunctions::block_structure& block, std::string& reason);
};

#endif
