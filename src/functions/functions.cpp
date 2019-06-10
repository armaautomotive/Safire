// Copyright (c) 2016 2017 2018 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/functions.h"
#include "wallet.h"
#include <boost/lexical_cast.hpp>
#include "ecdsacrypto.h"
#include "global.h"
#include <sstream>
#include "functions/selector.h"
#include "blockdb.h"
#include "functions/chain.h"
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <iostream>
#include "log.h"

/**
 * tokenClose
 *
 * Description: Find closing token for a structure in a string.
 */
int CFunctions::tokenClose(std::string content, std::string open, std::string close, int start){
    std::size_t start_i = content.find(open);
    std::size_t end_i = content.find(close);
    if (start_i!=std::string::npos && end_i!=std::string::npos){
        std::string section = content.substr (start_i + 1, end_i - start_i -1);
        
    }
    return 0;
}


/**
* recordJSON 
*
* Description: generate a json string representation of a record structure.
*/
std::string CFunctions::recordJSON(record_structure record){
	std::string json = "{\"record\":{ \"network\":\"" + record.network + "\","+
                "\"time\":\"" + record.time + "\"," +
                "\"name:\":\"" + record.name + "\"," +
                "\"typ\":\"" + boost::lexical_cast<std::string>(record.transaction_type) + "\"," +
                "\"amt\":\"" + boost::lexical_cast<std::string>(record.amount) + "\"," +
                "\"fee\":\"" + boost::lexical_cast<std::string>(record.fee) + "\"," +
                "\"sndkey\":\"" + record.sender_public_key + "\"," +
                "\"rcvkey\":\"" + record.recipient_public_key + "\"," +
		"\"value\":\"" + record.value + "\"," +
                "\"hash\":\"" + record.hash + "\"," +
                "\"sig\":\"" + record.signature + "\"" +
                "}}\n";
	return json;
}

/**
 * addToQueue
 *
 * Description: Add queue record to file. Data access object function.
 * @param time:
 * @param transaction_type: [add_user, issue_currency, transfer] designates action of this record.
 * @param amount: double amount of currency to be sent.
 * @param
 */
int CFunctions::addToQueue(record_structure record){
    std::ofstream outfile;
    outfile.open("queue.dat", std::fstream::out | std::fstream::app | std::ios_base::app);
    outfile << recordJSON(record);
    outfile.close();
    return 1;
}

void CFunctions::printQueue(){
    std::vector<record_structure> records = parseQueueRecords(); 
    for(int i = 0; i < records.size(); i++){
        CFunctions::record_structure record = records.at(i);
        std::cout << recordJSON(record);
    }
}

/**
 * parseQueueRecords
 *
 * Description: Read queued records from file into a vector of structures.
 * @return vector or record_structures
 */
std::vector<CFunctions::record_structure> CFunctions::parseQueueRecords(){
    std::vector<record_structure> records;
    std::ifstream infile("queue.dat");
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        CFunctions::record_structure record;
        record = parseRecordJson(line);
   
        // TODO: delete record if it allready exists in the queue???? ***
     
        records.push_back (record);
    }

    // TEMP: delete queue file.
    std::ofstream ofs;
    ofs.open("queue.dat", std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    return records;
}

/**
* parseOneRecordStructure  - extractOneQueueRecord
*
*
*/
CFunctions::record_structure CFunctions::extractOneQueueRecord(){
    CFunctions::record_structure record;


    return record; 
}

/**
* existsInQueue
*
* Description: Check if a given record allready exists in the queue file.
*/
int CFunctions::existsInQueue(record_structure record){
    
    
    return 0;
}

/**
* getRecordsInQueue
*
* Description: Get a list 
*/
int CFunctions::getRecordsInQueue( int limit ){
    
    
}

/**
 * validateRecord
 *
 * Description: validate record is formatted, the hashes match and balances are sufficient.
 */
int CFunctions::validateRecord(record_structure record){
    //
    // Read block
    
    return 0;
}

/**
 *
 *
 */
int CFunctions::generateBlock(std::vector<record_structure> records, std::string time ){
    
}

/**
 * blockJSON
 *
 * Description: generate json string from block data.
 *
 * @param: block_structure
 * @return string json.
 */
std::string CFunctions::blockJSON(CFunctions::block_structure block){
    std::stringstream ss;
    ss << "{\"block\":{" <<
        "\"creator_key\":\"" << block.creator_key << "\","  << 
        "\"network\":\"" << block.network << "\"," <<
        "\"number\":\"" << block.number << "\"," <<
        "\"time\":\"" << block.time << "\"," <<
        "\"previous_block_id\":\"" << boost::lexical_cast<std::string>(block.previous_block_id) << "\"," <<
        "\"previous_block_hash\":\"" << block.previous_block_hash + "\"," <<
        "\"hash\":\"" << block.hash + "\"," <<
        "\"records\":{\n";
    // Loop though block records
    for(int i = 0; i < block.records.size(); i++ ){
        CFunctions::record_structure record = block.records.at(i);
        ss << recordJSON(record);
    }
    ss << "}}}\n";

    std::string json = ss.str();
    return json;
}

/**
* addToBlockFile *** DEPRICATE ***
*
* Description: Add a block to the chain file.
*   text file and db.
*/
int CFunctions::addToBlockFile( CFunctions::block_structure block ){
    CBlockDB blockDB;
    //blockDB.AddBlock(block);

    time_t t = time(0); // get time now
    struct tm * now = localtime(&t);
    int year = (now->tm_year + 1900);
    
    std::stringstream ss;
    ss << "block_" << year << ".dat";
    std::string file_path = ss.str();
    
    std::ofstream outfile;
    outfile.open(file_path, std::ios_base::app);
    
    std::string json = blockJSON(block);
    outfile << json;
    outfile.close();

    latest_block = block; // TEMP this may change     
 
    return 0;
}

/**
* getLastBlock
*
* Description: retrieve the last block from the blockchain file.
*     Used when generating a new block to add the correct previoud block hash.
*
* @param
* @return 
*/
CFunctions::block_structure CFunctions::getLastBlock(std::string network){
   return latest_block;
}


/**
* parseSection
*
* Description: Extract a section from a string and convert it to a double.
* TODO use parseSectionString 
*/
double CFunctions::parseSectionDouble(std::string content, std::string start, std::string end){
    std::string section = parseSectionString(content, start, end);
    double result = ::atof(section.c_str());
    /* 
    std::size_t start_i = content.find(start);
    if (start_i!=std::string::npos){
	std::size_t end_i = content.find(end, start_i + start.length() + 1);        
        if(end_i!=std::string::npos){
             std::string section = content.substr(start_i + start.length(), end_i - (start_i + start.length()) - 0);
             double value = ::atof(section.c_str());
             return value;
        }
    }
    */
    return result;
}

long CFunctions::parseSectionLong(std::string content, std::string start, std::string end){
    std::string section = parseSectionString(content, start, end);
    long result = ::atol(section.c_str());
    return result;
}

int CFunctions::parseSectionInt(std::string content, std::string start, std::string end){
	std::string section = parseSectionString(content, start, end);
	int result = ::atoi(section.c_str());
	return result;
}

std::string CFunctions::parseSectionString(std::string content, std::string start, std::string end){
    std::string result = "";
    std::size_t start_i = content.find(start);
    if (start_i!=std::string::npos){
        std::size_t end_i = content.find(end, start_i + start.length() + 0); // + 1
        if(end_i!=std::string::npos){
             std::string section = content.substr(start_i + start.length(), end_i - (start_i + start.length()) - 0);
             //double value = ::atof(section.c_str());
             return section;
        }
    }
    return "";
}

// Find end of block tag position
// digest
std::string CFunctions::parseSectionBlock(std::string & content, std::string start, std::string open, std::string close){
    std::string result = "";
    int blockDepth = 0;
    bool started = false;
    
    std::size_t start_i = content.find(start);
    if (start_i!=std::string::npos){
        
        for(int i = start.length(); i < content.length(); i++){
            if(content[start_i + i] == '{'){ // open
                blockDepth++;
                started = true;
            }
            if(content[start_i + i] == '}'){ // close
                blockDepth--;
            }
            if( blockDepth == 0 && started == true){
                result = content.substr(start_i + start.length() + 1, i - start.length() - 1);
                //std::cout << "  ---  block  " << section << std::endl;
                
                content = content.substr(i - start.length(), std::string::npos);
                break;
            }
        }
    }
    return result;
}


/**
 * loadChain
 *
 * Description: parse the block chain stored in levelDB managed by CBlockDB.
 */
void CFunctions::scanChain(std::string my_public_key, bool debug){
    //std::cout << "scanChain \n";
    CECDSACrypto ecdsa;
    CSelector selector;
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    int i = 1;
    
    double updatedBalance = 0.0;
    
    if(firstBlockId > -1){
        CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
        //CFunctions::block_structure previous_block = block;
        while(block.number > 0){
            
            if(debug){
                std::cout << "  block " << i << "    number: " << block.number << "\n";
                std::cout << "  previous block: " << block.previous_block_id << "\n";
                std::cout << "  creator: " << block.creator_key << "\n";

            }
            latest_block = block;
            
            // Evaluate:
            std::vector<CFunctions::record_structure> records = block.records;
            //std::cout << "   records " << records.size() << " \n";
            for(int r = 0; r < records.size(); r++){
                CFunctions::record_structure record = records[r];
                //std::cout << "   record \n";
                
                //std::cout << " pub " << record.sender_public_key << " mykey " << my_public_key << "\n";
                //std::cout << " pubtype " << record.transaction_type << " mykey " << CFunctions::JOIN_NETWORK << "\n";
                if(record.sender_public_key.compare(my_public_key) == 0 &&
                   record.transaction_type == CFunctions::JOIN_NETWORK){ // TODO: and network name matches
                    joined = true;
                }
                
                if(record.transaction_type == CFunctions::JOIN_NETWORK){ // And block + previous chain is valid
                    user_count++;
                    selector.addUser(record.sender_public_key);
                }
                
                // Recipient of transfer
                if( (record.transaction_type == CFunctions::TRANSFER_CURRENCY ||
                     record.transaction_type == CFunctions::ISSUE_CURRENCY) &&
                   record.recipient_public_key.compare(my_public_key) == 0 ){
                    updatedBalance += record.amount;
                }
                
                // subtract sent payments from balance
                if(record.transaction_type == CFunctions::TRANSFER_CURRENCY &&
                   record.sender_public_key.compare(my_public_key) == 0 &&
                   record.recipient_public_key.compare(my_public_key) != 0){ // subtract sent from wallet to anyone but self.
                    updatedBalance -= record.amount;
                    updatedBalance -= record.fee;
                    //std::cout << " sent " << std::endl;
                }
                
                // collect fees
                if(record.transaction_type == CFunctions::TRANSFER_CURRENCY || record.transaction_type == CFunctions::VOTE){
                    updatedBalance += record.fee;
                }
                
                // Tally currency supply
                if(record.transaction_type == CFunctions::ISSUE_CURRENCY){
                    currency_circulation += record.amount;
                }
                
                if(debug){
                    std::string sender = record.sender_public_key;
                    std::string recipient = record.recipient_public_key;
                    if(sender.length() > 8){
                        sender = sender.erase(8);
                    }
                    if(recipient.length() > 8){
                        recipient = recipient.erase(8);
                    }
                    std::cout << "        ";
                    if(record.transaction_type == CFunctions::ISSUE_CURRENCY){
                        std::cout << "ISSUE   ";
                    }
                    if(record.transaction_type == CFunctions::JOIN_NETWORK){
                        std::cout << "JOIN_NET";
                    }
                    if(record.transaction_type == CFunctions::TRANSFER_CURRENCY){
                        std::cout << "TRANSFER";
                    }
                    if(record.transaction_type == CFunctions::CARRY_FORWARD){
                        std::cout << "CARRY_F ";
                    }
                    if(record.transaction_type == CFunctions::PERIOD_SUMMARY){
                        std::cout << "SUMMARY ";
                    }
                    if(record.transaction_type == CFunctions::VOTE){
                        std::cout << "VOTE    ";
                    }
                    if(record.transaction_type == CFunctions::HEART_BEAT){
                        std::cout << "HEART BT";
                    }
                    std::cout << "  sender: " << sender << " -> " << recipient << " amt: " << record.amount;
                    std::cout << " time: " << record.time << " net: " << record.network ;
                    std::cout << " typ " << boost::lexical_cast<std::string>(record.transaction_type) << "  ";
                    std::cout << " fee " << boost::lexical_cast<std::string>(record.fee) << " ";
                    
                    std::string validate_record_hash = getRecordHash(record);
                    if(validate_record_hash.compare(record.hash) == 0){
                        std::cout << ANSI_COLOR_GREEN <<  " [HASH_OK]" << ANSI_COLOR_RESET;
                    } else {
                        std::cout << std::endl << ANSI_COLOR_RED << "        [HASH_ERROR] " << validate_record_hash  << " == " << record.hash << ANSI_COLOR_RESET;
                        
                    }
                    // print signature validation ... TODO
                    // VerifyMessage()
                    //std::cout << "\n        ***  sig hash  " << record.hash << " sig " << record.signature  <<  " key  "<<  record.sender_public_key << std::endl;
                    //std::cout << " ---  " << record.sender_public_key << std::endl;
                    try {
                        int r = ecdsa.VerifyMessageCompressed(record.hash, record.signature, record.sender_public_key);
                        //std::cout << " 3333 ";
                        if(r){
                            std::cout << ANSI_COLOR_GREEN << "    [SIG_OK]" << ANSI_COLOR_RESET;
                        } else {
                            std::cout << ANSI_COLOR_RED << "    [SIG_ERROR]" << ANSI_COLOR_RESET;
                        }
                    } catch (...){
                        std::cout << "ERROR" << std::endl;
                    }
                    
                    std::cout << std::endl;
                }
                
            }
            
            block = blockDB.getNextBlock(block);
            
            // blockDB.getNextBlock has a bug that can't detect the end of the chain. Catch that case here.
            if(block.number == latest_block.number){
                CFunctions::block_structure blank_block;
                block = blank_block;
            }
            i++;
            
            //std::cout << " i " << i << " number:" << block.number << std::endl;
            
        }
        // No more blocks in chain.
        //std::cout << "End of chain." << std::endl;
        
        // Update latest block record
        long latestBlockId = blockDB.getLatestBlockId();
        if(latest_block.number > latestBlockId){
            blockDB.setLatestBlockId(latest_block.number);
            // check this... values don't seem correct
            
            std::cout << " set latest block id " << latest_block.number << std::endl;
        }
        
        // Update balance variable
        balance = updatedBalance;
    }
}

/**
 * parseBlockFile   *** DEPRICATE ***
 *
 * This function is being depricated because file storage of the blockchain makes it difficult to
 * access specific records and insert or re order records received out of sequence from the network.
 *
 * Description: Read the block file into in memory data structure.
 *
 * TODO: Keep track of a file pointer index of the last part of the file parsed
 * 	so that on subsiquent parse operations it only has to read new sections of the file?
 */
int CFunctions::parseBlockFile( std::string my_public_key, bool debug ){
    CFileLogger log;
    std::ostringstream logStream;
    logStream << " WARNING: CALLING DEPRICATED FUNCTION parseBlockFile() \n";
    log.log(logStream.str());
    
    CECDSACrypto ecdsa;
    CSelector selector;
    CChain chain;  // DEPRICATE
    time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
    int year = (now->tm_year + 1900);

    std::string publicKey;
    std::string privateKey;
    CWallet wallet;
    wallet.read(privateKey, publicKey);
    my_public_key = publicKey;

    std::stringstream ss;
    ss << "block_" << year << ".dat";
    std::string file_path = ss.str();

    //CFunctions::block_structure block;
    currency_circulation = 0;
    balance = 0;
    std::string content = "";
    user_count = 0;

    std::vector<record_structure> records;
    std::ifstream infile(file_path);
    std::string line;
    while (std::getline(infile, line))
    {
        //std::istringstream iss(line);
        //CFunctions::record_structure record;

        content += line;
        //std::cout << "line" << std::endl;
        //std::cout << "  PARSE BLOCKCHAIN:  " << line << " " << std::endl;

        // Parse content into data structures and strip it from content string as it goes. 

        // TODO: use parseBlockJson() function here
        // vector blocks = parseBlockJson()

        //size_t n = std::count(s.begin(), s.end(), '_');

        // Read the first block in the file.
        int parenDepth = 0;
        std::size_t start_i = content.find("{");
        if (start_i!=std::string::npos){
            //std::string section = content.substr (start_i + 1, end_i - start_i -1);

            for(int i = 0; i < content.length(); i++){
                if(content[start_i + i] == '{'){
                    parenDepth++;
                    //std::cout << "  +:  " << " " << i << " d: " << parenDepth  << std::endl; 
                }
                if(content[start_i + i] == '}'){ 
                    parenDepth--;
                    //std::cout << "  -:  " << " " << i << " d: " << parenDepth  << std::endl;  
                }
                if( parenDepth == 0){
                    //std::cout << "  Found end of block  " << parenDepth << std::endl; 
                
                    std::string block_section = content.substr(start_i, i + 1);
                    //std::cout << "  ---  block  " << section << std::endl;
                    //std::cout << "    - block read " << start_i << std::endl;

                    // Populate block and its records....
                    // "number":0","time":"","hash":"","records"
                    // {"record": 

                    latest_block.records.clear();
                    latest_block.creator_key = parseSectionString(block_section, "\"creator_key\":\"", "\"");
                    latest_block.number = parseSectionLong(block_section, "\"number\":\"", "\"");
                    std::string hash = parseSectionString(block_section, "\"hash\":\"", "\"" );
                    latest_block.hash = hash;

                    //std::cout << "  ---  latest_block.number  " << latest_block.number << std::endl;
                    //std::cout << "    hash: " << hash << std::endl; 

                    //std::string records_section = parseSection(section, "\"records\"", "}");
                    std::string records_section = parseSectionBlock(block_section, "\"records\":", "{", "}");
                    //std::cout << "  ---  records  " << records_section << "\n" << std::endl;
                    std::string record_section = parseSectionBlock(records_section, "\"record\":", "{", "}");
                    //std::cout << "  ---  record  " << record_section << "\n" << std::endl;

                    if(debug){
                        std::cout << "    Block " << latest_block.number << " creator: " << latest_block.creator_key << std::endl;
                    }

                    while( record_section.compare("") != 0 ){
                        CFunctions::record_structure record;

                        record = parseRecordJson(record_section);
                        


                        if(record.sender_public_key.compare(my_public_key) == 0 && record.transaction_type == CFunctions::JOIN_NETWORK){ // TODO: and network name matches
                            joined = true;
                        }

                        if(record.transaction_type == CFunctions::JOIN_NETWORK){ // And block + previous chain is valid
                            user_count++;
                            selector.addUser(record.sender_public_key);
                        }

                        // Recipient of transfer
                        if( (record.transaction_type == CFunctions::TRANSFER_CURRENCY ||
                                        record.transaction_type == CFunctions::ISSUE_CURRENCY) &&
                                         record.recipient_public_key.compare(my_public_key) == 0 ){
                            balance += record.amount;
                        }

                        // subtract sent payments from balance
                        if(record.transaction_type == CFunctions::TRANSFER_CURRENCY &&
                                         record.sender_public_key.compare(my_public_key) == 0 &&
                                         record.recipient_public_key.compare(my_public_key) != 0){ // subtract sent from wallet to anyone but self.
                            balance -= record.amount;
                                            balance -= record.fee;
                            //std::cout << " sent " << std::endl;
                        }

                        // collect fees
                        if(record.transaction_type == CFunctions::TRANSFER_CURRENCY || record.transaction_type == CFunctions::VOTE){
                           balance += record.fee;
                        }

                        // Tally currency supply
                        if(record.transaction_type == CFunctions::ISSUE_CURRENCY){
                            currency_circulation += record.amount;
                        }
                        
                        latest_block.records.push_back(record);
                        if(debug){
                            std::string sender = record.sender_public_key;
                            std::string recipient = record.recipient_public_key;
                            if(sender.length() > 8){
                                sender = sender.erase(8);
                            }
                            if(recipient.length() > 8){
                                recipient = recipient.erase(8);
                            } 
                            std::cout << "        ";
                            if(record.transaction_type == CFunctions::ISSUE_CURRENCY){ 
                                std::cout << "ISSUE   ";
                            }
                            if(record.transaction_type == CFunctions::JOIN_NETWORK){ 
                                std::cout << "JOIN_NET";
                            }
                            if(record.transaction_type == CFunctions::TRANSFER_CURRENCY){   
                                std::cout << "TRANSFER";
                            }
                            if(record.transaction_type == CFunctions::CARRY_FORWARD){
                                std::cout << "CARRY_F ";
                            }
                            if(record.transaction_type == CFunctions::PERIOD_SUMMARY){
                                std::cout << "SUMMARY ";
                            } 
                            if(record.transaction_type == CFunctions::VOTE){
                                std::cout << "VOTE    ";
                            }
                            if(record.transaction_type == CFunctions::HEART_BEAT){
                                std::cout << "HEART BT";
                            }
                            std::cout << "  sender: " << sender << " -> " << recipient << " amt: " << record.amount;
                            std::cout << " time: " << record.time << " net: " << record.network ;
                            std::cout << " typ " << boost::lexical_cast<std::string>(record.transaction_type) << "  ";
                            std::cout << " fee " << boost::lexical_cast<std::string>(record.fee) << " ";  

                            std::string validate_record_hash = getRecordHash(record);
                            if(validate_record_hash.compare(record.hash) == 0){
                                std::cout << ANSI_COLOR_GREEN <<  " [HASH_OK]" << ANSI_COLOR_RESET; 
                            } else {
                                std::cout << std::endl << ANSI_COLOR_RED << "        [HASH_ERROR] " << validate_record_hash  << " == " << record.hash << ANSI_COLOR_RESET;
                            
                            }
                            // print signature validation ... TODO 
                            // VerifyMessage()
                            //std::cout << "\n        ***  sig hash  " << record.hash << " sig " << record.signature  <<  " key  "<<  record.sender_public_key << std::endl;
                            //std::cout << " ---  " << record.sender_public_key << std::endl;
                            try { 
                            int r = ecdsa.VerifyMessageCompressed(record.hash, record.signature, record.sender_public_key);
                            //std::cout << " 3333 ";
                            if(r){
                                std::cout << ANSI_COLOR_GREEN << "    [SIG_OK]" << ANSI_COLOR_RESET;
                            } else {
                                std::cout << ANSI_COLOR_RED << "    [SIG_ERROR]" << ANSI_COLOR_RESET;
                            }
                            } catch (...){
                                std::cout << "ERROR" << std::endl;
                            }

                            std::cout << std::endl;
                        } 

                        // record_section = parseSectionBlock(records_section, "\"record\":", "{", "}");
                        record_section = parseSectionBlock(records_section, "\"record\":", "{", "}");
                    }
                   	
                    // Is block valid???
                    //
                    if(debug){
                        std::cout << "    Validation [n/a] " << std::endl;
                        // latest_block.hash
                        std::cout << "    Hash " << latest_block.hash << " prev " << latest_block.previous_block_hash << std::endl;
 
                    }
                    chain.setLatestBlock(latest_block);  
                    
                    content = content.substr(start_i + i, content.length()); // strip out processed block
                }


                //start_i = content.find("{", start_i + 1);
            } // for char in content

            //start_i = content.find("{", start_i + 1); 
        } // if { found in line 

    } // end for each file line

    return 0;
}


/**
 * parseBlockJson TODO: move this into a JSON class?
 *
 * Description: parse json into block structures.
 */
std::vector<CFunctions::block_structure> CFunctions::parseBlockJson(std::string block_json){
    //std::cout << " parseBlockJson: " << block_json  << std::endl;    
    std::vector<CFunctions::block_structure> blocks;
    CFunctions::block_structure block;
    std::string content = block_json;
    int parenDepth = 0;
    std::size_t start_i = content.find("{");
    if(start_i!=std::string::npos){
        CFunctions::block_structure latest_block;
        for(int i = 0; i < content.length(); i++){
            if(content[start_i + i] == '{'){
                parenDepth++;
                //std::cout << "  +:  " << " " << i << " d: " << parenDepth  << std::endl;
            }
            if(content[start_i + i] == '}'){
                parenDepth--;
                //std::cout << "  -:  " << " " << i << " d: " << parenDepth  << std::endl;
            }
            if(parenDepth == 0){
                std::string block_section = content.substr(start_i, i + 1);

                //std::cout << " block_section " << block_section << std::endl;                

                latest_block.records.clear();
                latest_block.creator_key = parseSectionString(block_section, "\"creator_key\":\"", "\""); 
                latest_block.number = parseSectionLong(block_section, "\"number\":\"", "\"");
                latest_block.previous_block_id = parseSectionLong(block_section, "\"previous_block_id\":\"", "\"");
                std::string hash = parseSectionString(block_section, "\"hash\":\"", "\"" );
                latest_block.hash = hash;
                std::string records_section = parseSectionBlock(block_section, "\"records\":", "{", "}");
                std::string record_section = parseSectionBlock(records_section, "\"record\":", "{", "}");
                while(record_section.compare("") != 0){
                    CFunctions::record_structure record;
                    record = parseRecordJson(record_section);
                    latest_block.records.push_back(record);
                    record_section = parseSectionBlock(records_section, "\"record\":", "{", "}"); 
                }
                if( latest_block.number > 0 && latest_block.hash.length() > 0 ){
                    blocks.push_back(latest_block);
                } else {
                    //std::cout << "  no " << blockJSON(latest_block)  << std::endl;
                }
                content = content.substr(start_i + i, content.length()); // strip out processed block
            }
        }
    }  
    //std::cout << blocks.size() << std::endl;
    return blocks;
}


/**
* parseRecordJson
*
* Description: parse json string into record structure.
* 
* @param json string
* @return record_structure
*/
CFunctions::record_structure CFunctions::parseRecordJson(std::string record_section){
    CFunctions::record_structure record;
    record.network = parseSectionString(record_section, "\"network\":\"", "\"");
    record.time = parseSectionString(record_section, "\"time\":\"", "\"");
    int transaction_type = parseSectionInt(record_section, "\"typ\":\"", "\"" );
    // transaction_type_strings JOIN_NETWORK, ISSUE_CURRENCY, TRANSFER_CURRENCY, CARRY_FORWARD, PERIOD_SUMMARY, VOTE
    // TODO: there has to be a better way to do this
    if(transaction_type == 0){
        record.transaction_type = CFunctions::JOIN_NETWORK;
    }
    if(transaction_type == 1){
        record.transaction_type = CFunctions::ISSUE_CURRENCY;
    }
    if(transaction_type == 2){
        record.transaction_type = CFunctions::TRANSFER_CURRENCY;
    }
    if(transaction_type == 3){
        record.transaction_type = CFunctions::CARRY_FORWARD;
    }
    if(transaction_type == 4){
        record.transaction_type = CFunctions::PERIOD_SUMMARY;
    }
    if(transaction_type == 5){
        record.transaction_type = CFunctions::VOTE;
    }
    if(transaction_type == 6){
        record.transaction_type = CFunctions::HEART_BEAT;
    }

    // address
    record.recipient_public_key = parseSectionString(record_section, "\"rcvkey\":\"", "\"" );
    record.sender_public_key = parseSectionString(record_section, "\"sndkey\":\"", "\"" );

    record.amount = parseSectionDouble(record_section, "\"amt\":\"", "\"");
    record.fee = parseSectionDouble(record_section, "\"fee\":\"", "\"");

    record.name = parseSectionString(record_section, "\"name\":\"", "\"");
    record.value = parseSectionString(record_section, "\"value\":\"", "\"");

    record.hash = parseSectionString(record_section, "\"hash\":\"", "\"");
    record.signature = parseSectionString(record_section, "\"sig\":\"", "\"");

    return record;
}


/**
* getBlockHash
*
* Description: Calculate a sha256 hash of the content of a block and it's records.
*
* @param block - record struct.
* @return string - sha256 hash value. 
*/
std::string CFunctions::getBlockHash(block_structure block){
    CECDSACrypto ecdsa;
    std::string block_content = block.network + 
        boost::lexical_cast<std::string>(block.number) +
        block.time +
        block.previous_block_hash +
        block.creator_key;
    std::vector<CFunctions::record_structure> records = block.records;
    for(int i = 0; i < records.size(); i++){
        CFunctions::record_structure record = records.at(i);
        if(record.hash.compare("") == 0){
            record.hash = getRecordHash(record);
        }
        std::stringstream ss;
        ss << block_content << record.hash;
        block_content = ss.str();
    } 
    std::string hash = "";
    char * c_hash = (char *)malloc( 65 );
    char *cstr = new char[block_content.length() + 1];
    strcpy(cstr, block_content.c_str());
    ecdsa.sha256(cstr, c_hash);
    hash = std::string(c_hash);
    delete [] cstr;
    free(c_hash);
    return hash;
}

/**
* GetRecordHash
*
* Description: Generate the record hash from its element values.
*
* @param: record_structure - data structure to 
* @return string - sha256 hash value 64 characters long.
*/
std::string CFunctions::getRecordHash(record_structure record){
    CECDSACrypto ecdsa;
    std::string record_content = record.network + 
        record.time + 
        boost::lexical_cast<std::string>(record.transaction_type) + 
        boost::lexical_cast<std::string>(record.amount) +
        boost::lexical_cast<std::string>(record.fee) +
        record.sender_public_key +
        record.recipient_public_key +
        record.name + 
        record.value;
    std::string hash = "";
    char * c_hash = (char *)malloc( 65 );
    char *cstr = new char[record_content.length() + 1];
    strcpy(cstr, record_content.c_str());
    ecdsa.sha256(cstr, c_hash);
    hash = std::string(c_hash);
    delete [] cstr;
    free(c_hash);
    return hash;
}


/**
* getRecordSignature
*
* Description: 
*/
std::string CFunctions::getRecordSignature(record_structure record){
    std::string signature;

    return signature;
}

std::string CFunctions::getBlockSignature(block_structure block){

	return "";
}

/**
 * DeleteAll
 *
 * Descirption: delete block data files.
 */
void CFunctions::DeleteAll(){
    boost::filesystem::path p = ".";
    std::string prefix("block_");
    std::string extension(".dat");
    for(auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(p), {})){
        std::ostringstream oss;
        oss << entry;
        std::string path = oss.str();
        //std::cout << path << "\n";
        std::size_t extension_found = path.find(extension);
        std::size_t prefix_found = path.find(prefix);
        if (extension_found != std::string::npos && prefix_found != std::string::npos){
            //std::cout << "DELETE: " << path << "\n";
            boost::filesystem::remove(entry);
        }
    }
    // "queue.dat"
    boost::filesystem::remove("queue.dat");
    boost::filesystem::remove("chain.index");
}
