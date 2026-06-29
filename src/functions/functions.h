// Copyright (c) 2016, 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_FUNCTIONS_H
#define SAFIRE_FUNCTIONS_H

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <ctime>

class CFunctions
{
private:
    //! Whether this private key is valid. We check for correctness when modifying the key
    //! data, so fValid should always correspond to the actual state.
    bool fValid;
    //CFunctions::block_structure latest_block;    
    
public:
    static const long HEARTBEAT_VALID_BLOCKS = 5760; // 24 hours at 15 seconds per block.
    static const long HEARTBEAT_RENEW_BLOCKS = 5520; // About 23 hours at 15 seconds per block.
    static const long CARRY_FORWARD_PERIOD_BLOCKS = 2102400; // About 1 year at 15 seconds per block.
    static const long CARRY_FORWARD_PRUNE_BLOCKS = 6307200; // About 3 years at 15 seconds per block.
    static constexpr double CARRY_FORWARD_REWARD = 1.0;
    static const int MAX_RECORD_NAME_BYTES = 64;
    static const int MAX_RECORD_VALUE_BYTES = 256;
    static const int MAX_RECORD_NETWORK_BYTES = 32;
    static const int MAX_RECORD_TIME_BYTES = 32;
    static const int MAX_RECORD_KEY_BYTES = 256;
    static const int MAX_RECORD_HASH_BYTES = 128;
    static const int MAX_RECORD_SIGNATURE_BYTES = 512;
    static const int MAX_RECORD_JSON_BYTES = 2048;
    static const int MAX_BLOCK_RECORDS = 1000;
    static const int MAX_BLOCK_JSON_BYTES = 131072;

    //! Construct an invalid private key.
    CFunctions()
    {
        balance = 0;
        joined = false;
        currency_circulation = 0;
        user_count = 0;
        active_heartbeat = false;
        heartbeat_renewal_due = false;
        chain_has_heartbeat_records = false;
        last_heartbeat_block = -1;
    }
    
    //! Destructor (again necessary because of memlocking).
    ~CFunctions()
    {
    }
   
    // ? Should these structures go elsewhere?
    enum transaction_types { JOIN_NETWORK, ISSUE_CURRENCY, TRANSFER_CURRENCY, CARRY_FORWARD, PERIOD_SUMMARY, VOTE, HEART_BEAT, UPDATE_NAME };

    // work in progress
    long last_parsed_block;

    struct chain_state_structure {
       std::string network_name;
       double balance;
       long users;
       long currency_supply; 
    };
    std::vector<CFunctions::chain_state_structure> chain_state;
    
    struct user_structure {
        std::string public_key = "";
        std::string name = "";              // Todo: optional public name.
        double balance = 0.0;
        long start_date = 0;
        long last_date = 0;
    };
    std::vector<CFunctions::user_structure> users; // Depricate for map based struct.
    std::map<std::string, CFunctions::user_structure> users_map;
    
    
    struct record_structure {
        std::string network;                // name of network this record belongs to
        std::string time;
        CFunctions::transaction_types transaction_type;
        double amount;                      // Amount to transfer from sender to recipient. 0 if type is add_user or vote
        double fee;	                        // transaction fee
        std::string sender_public_key;		// Sender key
        std::string recipient_public_key;	// Recipient key
        std::string hash;                   // hash of values.
        std::string signature;              // signature of hash with sender private key.
        std::string name; 		            // Meta (User name or transfer message)
        std::string value;                  // Meta (vote description)
        bool internal_validated = false;	// local toggle to indicate the record has been internally validated.
    };
    
    struct block_structure {
        std::string network;                // name of network block belongs to.
        long number;                     	// sequential block number
        std::string time;              		// time block created
        long file_index;                 	// bytes into block file for fast lookup.
        //std::string block_hash;           // sha256 hash of all record hashes in this block.
        long previous_block_id;             //
        std::string previous_block_hash;   	// ???  
        std::string block_records;         	// string value of all records in block.
        std::string creator_key;            // public key of user that creates the block.
        std::string hash;                   // sha256 hash of block content and it's records.
        std::string signature;          	// signature of block hash with block creator
        std::vector<CFunctions::record_structure> records;
        bool internal_validated = false;
    };
    
    struct peer_structure {
        std::string ip;
        bool active = true;
    };

    std::string recordJSON(record_structure record);
    std::string blockJSON( CFunctions::block_structure block); 

    int tokenClose(std::string content, std::string open, std::string close, int start);
    
    int addToQueue(record_structure record);
    std::vector<record_structure> peekQueueRecords();
    std::vector<record_structure> parseQueueRecords();
    int existsInQueue(record_structure record);
    int getRecordsInQueue( int limit );
    int validateRecord(record_structure record);
    bool isRecordSizeValid(record_structure record);
    std::string recordSizeError(record_structure record);
    int generateBlock( std::vector<CFunctions::record_structure> records, std::string time );
    int addToBlockFile( block_structure block );
    double parseSectionDouble(std::string content, std::string start, std::string end);
    long parseSectionLong(std::string content, std::string start, std::string end);
    
    void scanChain(std::string my_public_key, bool debug);
    int parseBlockFile( std::string my_public_key, bool debug );
    
    int parseSectionInt(std::string content, std::string start, std::string end);
    std::string parseSectionBlock(std::string & content, std::string start, std::string open, std::string close);
    std::string parseSectionString(std::string content, std::string start, std::string end);
    std::string getBlockHash(block_structure block);
    std::string getRecordHash(record_structure record);
    std::string getRecordSignature(record_structure record);
    std::string getBlockSignature(block_structure block);
    CFunctions::block_structure getLastBlock(std::string network);
    CFunctions::record_structure extractOneQueueRecord();
    CFunctions::record_structure parseRecordJson(std::string record_section);
    std::vector<CFunctions::block_structure> parseBlockJson(std::string block_json);
    void printQueue();

    void DeleteAll();
    
    bool IsChainUpToDate();
    double SyncProgress();
    
    std::string userJSON(CFunctions::user_structure user);
    CFunctions::user_structure parseUserJson(std::string user_json);
 
    CFunctions::block_structure latest_block;
    double balance; // wallet balance
    bool joined;
    double currency_circulation;
    double user_count;
    bool active_heartbeat;
    bool heartbeat_renewal_due;
    bool chain_has_heartbeat_records;
    long last_heartbeat_block;
};

#endif // SAFIRE_FUNCTIONS_H
