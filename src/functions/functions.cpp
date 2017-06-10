// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/functions.h"
#include <boost/lexical_cast.hpp>

/**
* recordJSON 
*
*
*/
std::string CFunctions::recordJSON(record_structure record){
	std::string json = "{\"record\":{\"time\":\"" + record.time + "\"," +
                "\"name:\":\"" + record.name + "\"," +
                "\"typ\":\"" +  boost::lexical_cast<std::string>(record.transaction_type) + "\"," +
                "\"amt\":" + boost::lexical_cast<std::string>(record.amount) + "," +
                "\"fee\":" + boost::lexical_cast<std::string>(record.fee) + "," +
                "\"sndkey\":\"" + record.sender_public_key + "\"," +
                "\"rcvkey\":\"" + record.recipient_public_key + "\"," +
                "\"sig\":\"" + record.message_signature + "\"" +
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
    outfile.open("queue.dat", std::ios_base::app);
/*
    outfile << "[" << record.time << "]" <<
        "[" << record.transaction_type << "]" <<
        "[" << record.amount << "]" <<
        "[" << record.sender_public_key << "]" <<
        "[" << record.recipient_public_key << "]" <<
        "[" << record.message_signature << "]" <<
        "\n";
*/
/*
	outfile << "{\"record\":{\"time\":\"" << record.time << "\"," <<
                "\"name:\":\"" <<record.name << "\"," <<
                "\"typ\":\"" << record.transaction_type << "\"," <<
                "\"amt\":" << record.amount << "," <<
                "\"fee\":" << record.fee << "," <<
                "\"sndkey\":\"" << record.sender_public_key << "\"," <<
                "\"rcvkey\":\"" << record.recipient_public_key << "\"," <<
                "\"sig\":\"" << record.message_signature << "\"" <<
                "}}\n";
*/
	outfile << recordJSON(record);
    outfile.close();
    return 1;
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
        
        
        std::size_t start = line.find("[");
        std::size_t end = line.find("]");
        if (start!=std::string::npos && end!=std::string::npos){
            std::string time = line.substr (start + 1, end-start -1);
            record.time = time;
            std::cout << "  Time:  " << time << " " << std::endl;
            
            start = line.find("[", end);
            end = line.find("]", end + 1);
            std::string type = line.substr (start + 1, end-start - 1);
            std::cout << "  type:  " << type << " " << std::endl;

            
        }
            
        //int a, b;
        //if (!(iss >> a >> b)) { break; } // error
        
        std::cout << "  Line:  " << line << " " << std::endl;
        // process pair (a,b)
        
        
        //record.time = "2017/06/02";
        
        
        records.push_back (record);
        
        
    }
    
    
    return records;
}

int CFunctions::existsInQueue(record_structure record){
    
    
    
    return 0;
}


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


int CFunctions::addToBlockFile( CFunctions::block_structure block ){
    
    time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
    int year = (now->tm_year + 1900);
    
    std::stringstream ss;
    ss << "block_" << year << ".dat";
    std::string file_path = ss.str();
    
    std::ofstream outfile;
    outfile.open(file_path, std::ios_base::app);
    outfile << "{\"block\":{" <<
	"\"number\":" << block.number << "\"," <<
	"\"time\":\"" << block.time << "\"," << 
	"\"hash\":\"" << block.block_hash << "\"," <<
	//"[" << block.transaction_type << "]" << 
	"\"records\":{\n";

    // Loop though block records
    for(int i = 0; i < block.records.size(); i++ ){
	CFunctions::record_structure record = block.records.at(i);
	/*
	outfile << " [" << record.time << "]" <<
        "[" << record.transaction_type << "]" <<
        "[" << record.amount << "]" <<
        "[" << record.sender_public_key << "]" <<
        "[" << record.recipient_public_key << "]" <<
        "[" << record.message_signature << "]" <<
        "\n";*/
	outfile << recordJSON(record);
    }

    outfile << "}}}\n";

    outfile.close();
    
    return 0;
}

int CFunctions::parseBlockFile(){
    
    return 0;
}
