// Copyright (c) 2016, 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_COMPRESSION_H
#define SAFIRE_COMPRESSION_H

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <ctime>

class CCompression
{
private:
    
public:
    //! Construct.
    CCompression()
    {
    }
    
    //! Destructor.
    ~CCompression()
    {
    }
    
    
    std::string string_compress_encode(const std::string &data);
    std::string string_decompress_decode(const std::string &data);
    
    
  
    
    
};

#endif // SAFIRE_COMPRESSION_H
