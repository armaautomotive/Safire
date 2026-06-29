// Copyright (c) 2026 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SAFIRE_NETWORK_CONFIG_H
#define SAFIRE_NETWORK_CONFIG_H

#include <cstdlib>
#include <fstream>
#include <string>

class CNetworkConfig
{
public:
    std::string network;
    long genesisBlock;
    std::string genesisHash;
    std::string defaultPeer;
    bool strictGenesis;

    CNetworkConfig()
    {
        network = "main";
        genesisBlock = -1;
        genesisHash = "";
        defaultPeer = "";
        strictGenesis = true;
    }

    bool hasExpectedGenesis() const
    {
        return strictGenesis && genesisBlock > 0 && genesisHash.length() > 0;
    }

    bool genesisMatches(long blockNumber, const std::string& blockHash) const
    {
        if(hasExpectedGenesis() == false){
            return true;
        }
        return genesisBlock == blockNumber && genesisHash.compare(blockHash) == 0;
    }

    bool save(const std::string& path = "safire.conf") const
    {
        std::ofstream outfile(path.c_str());
        if(!outfile.good()){
            return false;
        }
        outfile << "network=" << network << "\n";
        outfile << "genesis_block=" << genesisBlock << "\n";
        outfile << "genesis_hash=" << genesisHash << "\n";
        outfile << "default_peer=" << defaultPeer << "\n";
        outfile << "strict_genesis=" << (strictGenesis ? "1" : "0") << "\n";
        outfile.close();
        return true;
    }

    static CNetworkConfig load(const std::string& path = "safire.conf")
    {
        CNetworkConfig config;
        std::ifstream infile(path.c_str());
        std::string line;
        while(std::getline(infile, line)){
            std::size_t separator = line.find("=");
            if(separator == std::string::npos){
                separator = line.find(":");
            }
            if(separator == std::string::npos){
                continue;
            }

            std::string key = trim(line.substr(0, separator));
            std::string value = trim(line.substr(separator + 1));
            if(key.compare("network") == 0){
                config.network = value;
            } else if(key.compare("genesis_block") == 0){
                config.genesisBlock = std::atol(value.c_str());
            } else if(key.compare("genesis_hash") == 0){
                config.genesisHash = value;
            } else if(key.compare("default_peer") == 0){
                config.defaultPeer = value;
            } else if(key.compare("strict_genesis") == 0){
                config.strictGenesis = value.compare("0") != 0 &&
                    value.compare("false") != 0 &&
                    value.compare("no") != 0;
            }
        }
        return config;
    }

private:
    static std::string trim(const std::string& value)
    {
        std::size_t start = value.find_first_not_of(" \t\r\n");
        if(start == std::string::npos){
            return "";
        }
        std::size_t end = value.find_last_not_of(" \t\r\n");
        return value.substr(start, end - start + 1);
    }
};

#endif // SAFIRE_NETWORK_CONFIG_H
