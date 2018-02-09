// Copyright (c) 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "platform.h"
#include "global.h"
#include <sstream>
#include <sstream>
#include <string>
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
    std::string path;
    const char *homeDir = getenv("HOME");
    //std::cout << " dir " << homeDir << "\n";
    std::stringstream ss;
    ss << homeDir << separator() << "safire";
    path = ss.str();
    const char* c_path = path.c_str();
    boost::filesystem::path dir(c_path);
    if(boost::filesystem::create_directory(dir))
    {
        std::cerr<< "Directory Created: "<< path <<std::endl;
    }
    return path;
}


