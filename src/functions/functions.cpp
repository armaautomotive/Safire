// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/functions.h"
#include <boost/lexical_cast.hpp>

/*
 {"record":{"time":"1497069518","name:":"","typ":"0","amt":0,"fee":0,"sndkey":"026321261876CFB360F94A7FDF3F2D4F6F9FC0CEDADF15AC3D30E182A82AF5D81E","rcvkey":"","sig":"DF9721B14253E380F9E6EF00A5DBF1DB74751BF7F7EBBF104E04FE462CBA27255144B900251E061ED16CAC9B572088B446B42B448E0A6A64D92E697FC5142960"}}

{"block":{"number":1","time":"","hash":"","records":{
{"record":{"time":"1497071981","name:":"","typ":"1","amt":1,"fee":-1.0367648569192718e-155,"sndkey":"","rcvkey":"026321261876CFB360F94A7FDF3F2D4F6F9FC0CEDADF15AC3D30E182A82AF5D81E","sig":"A1BBB5C4B1FD3081EBE276627DE088B76B4932650CF6A3EA9D31EFE5B03F106AD5D8C32A38537F9D09B72220D4FF54106F30BF17C8068995AF221D1FAB5EE183"}}
}}}

*/

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
*
*/
std::string CFunctions::recordJSON(record_structure record){
	std::string json = "{\"record\":{\"time\":\"" + record.time + "\"," +
                "\"name:\":\"" + record.name + "\"," +
                "\"typ\":\"" + boost::lexical_cast<std::string>(record.transaction_type) + "\"," +
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
    outfile << recordJSON(record);
    outfile.close();
    return 1;
}


/**
* parseRecordJson
*
* Description: 
*/
CFunctions::record_structure parseRecordJson(std::string json){
	CFunctions::record_structure record;

	std::size_t start = json.find("time\":\"");
        std::size_t end = json.find("]");
        if (start!=std::string::npos && end!=std::string::npos){
            std::string time = json.substr (start + 1, end-start -1);
            record.time = time;
            std::cout << "  Time:  " << time << " " << std::endl;

            start = json.find("[", end);
            end = json.find("]", end + 1);
            std::string type = json.substr (start + 1, end-start - 1);
            std::cout << "  type:  " << type << " " << std::endl;


        }

	return record;
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
	outfile << recordJSON(record);
    }

    outfile << "}}}\n";

    outfile.close();
    
    return 0;
}

int CFunctions::parseBlockFile(){
    time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
    int year = (now->tm_year + 1900);

    std::stringstream ss;
    ss << "block_" << year << ".dat";
    std::string file_path = ss.str();

    //CFunctions::block_structure block;

    std::vector<record_structure> records;
    std::ifstream infile(file_path);
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        //CFunctions::record_structure record;

	std::cout << "  PARSE BLOCKCHAIN:  " << line << " " << std::endl;

        std::size_t start = line.find("[");
        std::size_t end = line.find("]");

    }


    
    return 0;
}
