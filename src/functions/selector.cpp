/**
* CSelector
* 
* The selector defines a network wide mechanism for identifying a single user 
* from the network durring a given block of time. This selected user can be granted authenticity
* by other piers for blocks submitted.
*/

// Copyright (c) 2016 2017 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/selector.h"
#include "global.h"
#include <ctime>


std::vector< std::string > CSelector::users;


/**
* syncronizeTime
*
* Description: calculate time offset between local machine and network. 
* Apply time difference when using local time so that the network is syncronized.
*/
void CSelector::syncronizeTime(){
    // Get time from server

    // get time from local system

    
}

long CSelector::getCurrentTimeBlock(){
    time_t  timev;
    time(&timev);
    long timeBlock = (long)(timev / 15); // 30 second blocks
    return timeBlock; 
}

/**
* isSelected
*
* Description: is a given user designated for the current time period.
*/
bool CSelector::isSelected(std::string publicKey){
    //std::cout << " X " << users.size() <<  std::endl;

    if(users.size() == 0){ // not loaded yet
        return false;
    }

    time_t  timev;
    time(&timev);
    //std::stringstream ss;
    //ss << timev;
    //std::string ts = ss.str();

    // TODO: get time from central or synced set of servers for syncronization.

    long timeBlock = (long)(timev / 15); // 30 second blocks
    long userIndex = timeBlock % users.size();

    std::string selectedUser = users.at(userIndex);
    //std::cout << " time " <<  "  " << timeBlock << " mod " << userIndex <<  " users " << users.size() << " " << selectedUser <<  std::endl; 

    // Current time to 30 seconds, hashed into one of the user set. 
    // If that user is the current user return true;

    // for(int i = 0; i < users.size(); i++){ 
    if(selectedUser.compare(publicKey) == 0){
        return true;
    }
    return false;
}


/**
*
*
* TODO: use hashmap instead of vector
*/
void CSelector::addUser(std::string user){
    //std::cout << " add " << user << std::endl;
    bool exists = false;
    for(int i = 0; i < CSelector::users.size(); i++){
        std::string existingUser = (std::string)CSelector::users[i];
        if( existingUser.compare(user) == 0 ){
            exists = true;
        }
    }
    if(exists == false){
        CSelector::users.push_back(user);
    }    
}
