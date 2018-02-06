// Copyright (c) 2018 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "functions/chain.h"
#include "global.h"
#include <sstream>
#include "blockdb.h"
#include <boost/lexical_cast.hpp>

CChain::CChain(){
    firstBlock = -1;
    latestBlock = -1;    
    loadFile();
}

bool CChain::fileExists(std::string fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

void CChain::setFirstBlock(CFunctions::block_structure block){
    firstBlock = block.number;
}    

void CChain::writeFile(){
    std::string indexContent = "firstBlock:" + boost::lexical_cast<std::string>(firstBlock) + "\n" +
            "latestBlock:" + boost::lexical_cast<std::string>(latestBlock) + "\n";

    //const char *homeDir = getenv("HOME");
    //std::cout << " dir " << homeDir << "\n";

    std::ofstream out("chain.index");
    out << indexContent;
    out.close();
}
    
long CChain::getFirstBlock(){
    return firstBlock;
}

void CChain::loadFile(){
    std::ifstream t("chain.index");
    std::string indexContent((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());

    //std::cout << " file: " << indexContent << std::endl;
    std::size_t firstBlockStart = indexContent.find("firstBlock:", 0);
    std::size_t firstBlockEnd   = indexContent.find("\n", firstBlockStart);
    std::size_t latestBlockStart  = indexContent.find("latestBlock:", 0);
    std::size_t latestBlockEnd    = indexContent.find("\n", latestBlockStart);
    if (firstBlockStart!=std::string::npos) {
        std::string content = indexContent.substr(firstBlockStart + 11, firstBlockEnd - firstBlockStart - 11);
        long result = ::atol(content.c_str());
        firstBlock = result; 
        content = indexContent.substr(latestBlockStart + 12, latestBlockEnd - latestBlockStart - 7);
        result = ::atol(content.c_str());
        latestBlock = result;
    }
}
    
void CChain::setLatestBlock(CFunctions::block_structure block){
    latestBlock = block.number;
}
    
long CChain::getLatestBlock(){
    return latestBlock;
}

