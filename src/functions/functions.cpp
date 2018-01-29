// Copyright (c) 2016 2017 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/functions.h"
#include <boost/lexical_cast.hpp>

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
    outfile.open("queue.dat", std::fstream::out | std::fstream::app | std::ios_base::app);
    outfile << recordJSON(record);
    outfile.close();
    return 1;
}


/**
* parseRecordJson
*
* Description: DEPRICATE ????? 
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
 *    DEPRICATE ????
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
        
        //std::cout << "  Line:  " << line << " " << std::endl;
        // process pair (a,b)
        
        //record.time = "2017/06/02";
        
        records.push_back (record);
    }
    return records;
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
        "\"network\":\"" << block.network << "\"," <<
	"\"number\":\"" << block.number << "\"," <<
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
* parseBlockFile
*
* Description: Read the block file into in memory data structure.
*
* TODO: Keep track of a file pointer index of the last part of the file parsed 
* 	so that on subsiquent parse operations it only has to read new sections of the file?
*/
int CFunctions::parseBlockFile( std::string my_public_key ){
    time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
    int year = (now->tm_year + 1900);

    std::stringstream ss;
    ss << "block_" << year << ".dat";
    std::string file_path = ss.str();

    //CFunctions::block_structure block;
    balance = 0;
    std::string content = "";

    std::vector<record_structure> records;
    std::ifstream infile(file_path);
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        //CFunctions::record_structure record;

        content += line;
        //std::cout << "line" << std::endl;
        //std::cout << "  PARSE BLOCKCHAIN:  " << line << " " << std::endl;

        // Parse content into data structures and strip it from content string as it goes. 

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
                    latest_block.number = parseSectionLong(block_section, "\"number\":\"", "\"");
                    std::string hash = parseSectionString(block_section, "\"hash\":\"", "\"" );
                    latest_block.block_hash = hash;

			//std::cout << "  ---  latest_block.number  " << latest_block.number << std::endl; 
                    //std::cout << "    hash: " << hash << std::endl; 

                    //std::string records_section = parseSection(section, "\"records\"", "}");
                    std::string records_section = parseSectionBlock(block_section, "\"records\":", "{", "}");
                    //std::cout << "  ---  records  " << records_section << "\n" << std::endl;
                    std::string record_section = parseSectionBlock(records_section, "\"record\":", "{", "}");
		    //std::cout << "  ---  record  " << record_section << "\n" << std::endl;	

                    while( record_section.compare("") != 0 ){
                        CFunctions::record_structure record;
                        
                        // "record":{"time":"1502772739","name:":"","typ":"1","amt":1,"fee":0,"sndkey":"","rcvkey":"026321261876CFB360F94A7FDF3F2D4F6F9FC0CEDADF15AC3D30E182A82AF5D81E","sig":""}
                        //"amt":1
                        double amount = parseSectionDouble(record_section, "\"amt\":\"", "\"");
			record.amount = amount;
                        //std::cout << "  ---  record amount  " << amount << std::endl;
                        
                        // address
                        //std::string to_address = parseSectionBlock(record_section, "rcvkey", "", "" );
			std::string rcvkey = parseSectionString(record_section, "\"rcvkey\":\"", "\"" );                       
			record.recipient_public_key = rcvkey;
			std::string sndkey = parseSectionString(record_section, "\"sndkey\":\"", "\"" );
			record.sender_public_key = sndkey;
			std::string name = parseSectionString(record_section, "\"name\":\"", "\"");
			record.name = name;
			std::string value = parseSectionString(record_section, "\"value\":\"", "\"");
			record.value = value;

			//std::cout << "        record send _" <<  sndkey  << "_ recv " << rcvkey <<  " amt " << amount << std::endl; 

			int record_type = parseSectionInt(record_section, "\"typ\":\"", "\"" );  
			if( rcvkey.compare( my_public_key ) == 0 && record_type == CFunctions::JOIN_NETWORK ){
				joined = true;
			}

			if( rcvkey.compare(my_public_key) == 0 ){
				balance += amount;
			}
			// todo subtract sent payments from balance 
			if( sndkey.compare(my_public_key) == 0 && rcvkey.compare( my_public_key ) != 0 ){ // subtract sent from wallet to anyone but self.
				balance -= amount;
				//std::cout << " sent " << std::endl;
			}

			if( record_type == CFunctions::ISSUE_CURRENCY ){
				currency_circulation += amount;
			}
                        
                        latest_block.records.push_back(record);
                   

			// record_section = parseSectionBlock(records_section, "\"record\":", "{", "}");  
                        record_section = parseSectionBlock(records_section, "\"record\":", "{", "}");
                    }
                   	
			// Is block valid???
			//  
                    
                    content = content.substr(start_i + i, content.length()); // strip out processed block
                }


                //start_i = content.find("{", start_i + 1);
            } // for char in content

            //start_i = content.find("{", start_i + 1); 
        } // if { found in line 

    } // end for each file line

    return 0;
}

std::string CFunctions::getBlockHash(block_structure block){
	return "";
}

