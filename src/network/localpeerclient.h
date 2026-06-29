#ifndef SAFIRE_LOCAL_PEER_CLIENT_H
#define SAFIRE_LOCAL_PEER_CLIENT_H

#include "functions/functions.h"
#include <map>
#include <set>
#include <string>
#include <vector>

class CLocalPeerClient
{
public:
    static const int PROTOCOL_VERSION = 1;

    struct push_result {
        int candidateBlocks;
        int pushedBlocks;
        long failedBlockId;
        std::string response;
    };

    struct peer_status {
        std::string url;
        long firstBlockId;
        std::string firstBlockHash;
        long latestBlockId;
        std::string latestBlockHash;
        int protocolVersion;
        int score;
        int successes;
        int failures;
        long lastSeenEpoch;
        long lastSuccessEpoch;
        long firstFailureEpoch;
        bool genesisMatch;
        bool reachable;
        std::string lastError;
    };

    static void setPeers(const std::vector<std::string>& peers);
    static bool addPeer(const std::string& peerUrl, bool persist = true);
    static std::vector<std::string> getPeers();
    static std::vector<peer_status> getPeerStatuses();
    static void discoverPeers();
    static void stop();

    static void syncThread(int argc, char* argv[]);
    static bool syncFromPeer(const std::string& peerUrl);
    static int pushToPeer(const std::string& peerUrl);
    static push_result pushToPeerDetailed(const std::string& peerUrl);
    static push_result pushFullChainToPeerDetailed(const std::string& peerUrl);
    static long getPeerLatestBlockId(const std::string& peerUrl);
    static long getBestPeerLatestBlockId();
    static bool isSyncedWithPeers();
    static bool syncNetworkTime();
    static long getNetworkTimeOffset();
    static void broadcastRecord(const CFunctions::record_structure& record);
    static void broadcastBlock(const CFunctions::block_structure& block);

private:
    static void savePeerCache();
    static void purgeUnavailablePeers();
    static std::vector<std::string> peers;
    static std::map<std::string, peer_status> peerStatuses;
    static std::set<std::string> configuredPeers;
    static bool running;
};

#endif
