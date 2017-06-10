// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAGNITE_FUNCTIONS_H
#define MAGNITE_FUNCTIONS_H

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>


class CFunctions
{
private:
    //! Whether this private key is valid. We check for correctness when modifying the key
    //! data, so fValid should always correspond to the actual state.
    bool fValid;
    
    
public:
    //! Construct an invalid private key.
    CFunctions()
    {
    }
    
    //! Destructor (again necessary because of memlocking).
    ~CFunctions()
    {
    }
   
    // ? Should these structures go elsewhere? 
    enum transaction_types { ADD_USER, ISSUE_CURRENCY, TRANSFER_CURRENCY, CARRY_FORWARD };
    
    struct record_structure {
        std::string time;
        CFunctions::transaction_types transaction_type;
        double amount;                      // 0 if type is add_user
        double fee;
	std::string sender_public_key;
        std::string recipient_public_key;
        std::string message_signature;		// Hash of time+tran_type+amount+
	std::string name; 			// Meta (User name or transfer message)      	 

	bool internal_validated = false;
    };
    
    struct block_structure {
        double number;                      // sequential block number
        std::string time;                   // time block created
        double file_index;                  // bytes into block file for fast lookup.
        std::string block_hash;             // sha256 hash of all record hashes in this block.
        std::string block_records;          // string value of all records in block.
	std::string creator_key;
	std::string creator_signature;
	std::vector<CFunctions::record_structure> records;

	bool internal_validated = false; 
    };
    
    struct peer_structure {
        std::string ip;
	bool active = true;
    };

    std::string recordJSON(record_structure record);
    
    int addToQueue(record_structure record);
    std::vector<record_structure> parseQueueRecords();
    int existsInQueue(record_structure record);
    int getRecordsInQueue( int limit );
    int validateRecord(record_structure record);
    int generateBlock( std::vector<CFunctions::record_structure> records, std::string time );
    int addToBlockFile( block_structure block );
    int parseBlockFile();
};




#endif // MAGNITE_FUNCTIONS_H
