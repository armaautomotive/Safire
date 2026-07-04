// Copyright (c) 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "platform.h"
#include "global.h"
#include <sstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <boost/filesystem.hpp>

inline char CPlatform::separator()
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}


/**
* getSafirePath
*
* Description: 
*/
std::string CPlatform::getSafirePath(){
    const char *homeDir = getenv("HOME");
    boost::filesystem::path dir;
    if(homeDir == NULL || std::string(homeDir).length() == 0){
        dir = boost::filesystem::path(".");
    }else{
        dir = boost::filesystem::path(homeDir) / "safire";
    }
    boost::filesystem::create_directories(dir);
    std::string path = dir.string();
    if(boost::filesystem::exists(dir))
    {
        std::cerr << "Safire path: " << path << std::endl;
    }
    return path;
}

