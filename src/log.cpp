// Copyright (c) 2018 Jon Taylor
// Website: http://safire.org
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "log.h"
#include <sstream>
#include <unistd.h>     // open and close
#include <sys/stat.h>   // temp because we removed util
#include <fcntl.h>      // temp removed util.h
#include <time.h>
#include <sstream>

/**
* log
*
* Description:
*/
void CFileLogger::log(std::string str){
    time_t t = time(0); // get time now
    struct tm * now = localtime(&t);
    int year = (now->tm_year + 1900);
    
    std::stringstream ss;
    ss << "safire_" << year << ".log";
    std::string file_path = ss.str();
    
    std::ofstream outfile;
    outfile.open(file_path, std::ios_base::app);
    
    outfile << str;
    outfile.close();
}

/**
 * clearLog
 *
 * Description:
 */
void CFileLogger::clearLog(){
    time_t t = time(0); // get time now
    struct tm * now = localtime(&t);
    int year = (now->tm_year + 1900);
    
    std::stringstream ss;
    ss << "safire_" << year << ".log";
    std::string file_path = ss.str();
    
}
