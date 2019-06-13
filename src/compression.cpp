
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

#include "base64.h"


std::string CCompression::string_compress_encode(const std::string &data)
{
    std::stringstream compressed;
    std::stringstream original;
    original << data;
    
    //boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    /*
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



/** Compress a STL string using zlib with given compression level and return
 * the binary data. */
std::string CCompression::compress_string(const std::string& str) // int compressionlevel
{
    int compressionlevel = Z_BEST_COMPRESSION;
    z_stream zs;                        // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit(&zs, compressionlevel) != Z_OK)
        throw(std::runtime_error("deflateInit failed while compressing."));
    
    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();           // set the z_stream's input
    
    int ret;
    char outbuffer[32768];
    std::string outstring;
    
    // retrieve the compressed bytes blockwise
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = deflate(&zs, Z_FINISH);
        
        if (outstring.size() < zs.total_out) {
            // append the block to the output string
            outstring.append(outbuffer,
                             zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
        std::ostringstream oss;
        oss << "Exception during zlib compression: (" << ret << ") " << zs.msg;
        throw(std::runtime_error(oss.str()));
    }
    
    
    // need to encode here
    std::string compressed_encoded = base64_encode(reinterpret_cast<const unsigned char*>(outstring.c_str()), outstring.length());
    return compressed_encoded;
    //return outstring;
}

/** Decompress an STL string using zlib and return the original data. */
std::string CCompression::decompress_string(const std::string& str)
{
    z_stream zs;                        // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));
    
    if (inflateInit(&zs) != Z_OK)
        throw(std::runtime_error("inflateInit failed while decompressing."));
    
    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();
    
    int ret;
    char outbuffer[32768];
    std::string outstring;
    
    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = inflate(&zs, 0);
        
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer,
                             zs.total_out - outstring.size());
        }
        
    } while (ret == Z_OK);
    
    inflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
        std::ostringstream oss;
        oss << "Exception during zlib decompression: (" << ret << ") "
        << zs.msg;
        throw(std::runtime_error(oss.str()));
    }
    
    return outstring;
}
