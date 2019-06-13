
// Copyright (c) 2016 2017 2018 2019 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compression.h"

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <iostream>

#include <stdio.h>
#include <string.h>  // for strlen
#include <assert.h>
#include "zlib.h"

std::string CCompression::string_compress_encode(const std::string &data)
{
    /*
    std::stringstream compressed;
    std::stringstream original;
    original << data;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    out.push(boost::iostreams::zlib_compressor());
    out.push(original);
    boost::iostreams::copy(out, compressed);
    
    // need to encode here
    std::string compressed_encoded = base64_encode(reinterpret_cast<const unsigned char*>(compressed.c_str()), compressed.length());
    
    return compressed_encoded.str();
    */
    return "";
}

std::string CCompression::string_decompress_decode(const std::string &data)
{
    /*
    std::stringstream compressed_encoded;
    std::stringstream decompressed;
    compressed_encoded << data;
    
    // first decode  then decompress
    std::string compressed = base64_decode(compressed_encoded);
    
    boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
    in.push(boost::iostreams::zlib_decompressor());
    in.push(compressed);
    boost::iostreams::copy(in, decompressed);
    return decompressed;
     */
    return "";
}
