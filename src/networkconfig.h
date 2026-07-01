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
    std::string publicPeerUrl;
    std::string storageProfile;
    bool strictGenesis;
    bool enableNatTraversal;

    CNetworkConfig()
    {
        network = "main";
        genesisBlock = -1;
        genesisHash = "";
        defaultPeer = "";
        publicPeerUrl = "";
        storageProfile = "desktop";
        strictGenesis = true;
        enableNatTraversal = false;
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
        outfile << "public_peer_url=" << publicPeerUrl << "\n";
        outfile << "storage_profile=" << normalizeStorageProfile(storageProfile) << "\n";
        outfile << "strict_genesis=" << (strictGenesis ? "1" : "0") << "\n";
        outfile << "enable_nat=" << (enableNatTraversal ? "1" : "0") << "\n";
        outfile.close();
        return true;
    }

    static std::string normalizeStorageProfile(const std::string& value)
    {
        std::string normalized = trim(value);
        for(std::size_t i = 0; i < normalized.length(); ++i){
            if(normalized.at(i) >= 'A' && normalized.at(i) <= 'Z'){
                normalized.at(i) = static_cast<char>(normalized.at(i) - 'A' + 'a');
            }
        }
        if(normalized.compare("server") == 0 || normalized.compare("full") == 0){
            return "server";
        }
        if(normalized.compare("mobile") == 0 || normalized.compare("phone") == 0){
            return "mobile";
        }
        return "desktop";
    }

    std::string normalizedStorageProfile() const
    {
        return normalizeStorageProfile(storageProfile);
    }

    long carryForwardPeriodBlocks() const
    {
        return 30L * 24L * 60L * 4L; // About 30 days at 15 seconds per block.
    }

    long carryForwardCheckpointAgeBlocks() const
    {
        std::string profile = normalizedStorageProfile();
        if(profile.compare("server") == 0){
            return carryForwardPeriodBlocks();
        }
        if(profile.compare("mobile") == 0){
            return 90L * 24L * 60L * 4L; // About 3 months.
        }
        return 365L * 24L * 60L * 4L; // About 1 year.
    }

    long pruneHorizonBlocks() const
    {
        std::string profile = normalizedStorageProfile();
        if(profile.compare("server") == 0){
            return 0;
        }
        return carryForwardCheckpointAgeBlocks();
    }

    bool prunesOldBlocks() const
    {
        return pruneHorizonBlocks() > 0;
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
            } else if(key.compare("public_peer_url") == 0 || key.compare("advertised_peer") == 0){
                config.publicPeerUrl = value;
            } else if(key.compare("storage_profile") == 0 || key.compare("storage") == 0){
                config.storageProfile = normalizeStorageProfile(value);
            } else if(key.compare("strict_genesis") == 0){
                config.strictGenesis = value.compare("0") != 0 &&
                    value.compare("false") != 0 &&
                    value.compare("no") != 0;
            } else if(key.compare("enable_nat") == 0 || key.compare("enable_nat_traversal") == 0){
                config.enableNatTraversal = value.compare("1") == 0 ||
                    value.compare("true") == 0 ||
                    value.compare("yes") == 0;
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
