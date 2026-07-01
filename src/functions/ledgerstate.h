// Copyright (c) 2026 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_LEDGERSTATE_H
#define SAFIRE_LEDGERSTATE_H

#include "functions/functions.h"
#include <map>
#include <set>
#include <string>
#include <vector>

class CBlockDB;

class CLedgerState
{
public:
    struct member_state {
        std::string public_key;
        std::string name;
        long join_block;
        long last_heartbeat_block;
        bool active;
    };

    struct state {
        std::map<std::string, double> balances;
        std::map<std::string, long> nonces;
        std::map<std::string, std::string> names;
        std::set<std::string> accepted_record_hashes;
        std::set<std::string> accepted_carry_forward_keys;
        std::vector<member_state> members;
        std::vector<std::string> active_member_keys;
        double issued_supply;
        double ledger_balance_total;
        double wallet_balance;
        bool joined;
        bool active_heartbeat;
        bool heartbeat_renewal_due;
        bool chain_has_heartbeat_records;
        long last_heartbeat_block;
        long first_block_id;
        long latest_block_id;
        long connected_block_count;
        CFunctions::block_structure latest_block;
    };

    static state build(CBlockDB& block_db, const std::string& wallet_public_key = "", long stop_block = -1);
    static double balanceAtBlock(CBlockDB& block_db, const std::string& public_key, long checkpoint_block);
};

#endif
