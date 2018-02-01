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
#include <ctime>


std::vector< std::string > CSelector::users;

bool CSelector::isSelected(std::string publicKey){
    //std::cout << " X " << std::endl;

    time_t  timev;
    time(&timev);
    std::stringstream ss;
    ss << timev;
    std::string ts = ss.str();
    std::cout << " time " << ts <<  "  " << users.size() <<  std::endl; 

    // Current time to 30 seconds, hashed into one of the user set. 
    // If that user is the current user return true;

    // for(int i = 0; i < users.size(); i++){ 

    return true;
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
