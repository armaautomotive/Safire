#ifndef SAFIRE_LOCAL_PEER_CLIENT_H
#define SAFIRE_LOCAL_PEER_CLIENT_H

#include "functions/functions.h"
#include <string>
#include <vector>

class CLocalPeerClient
{
public:
    static void setPeers(const std::vector<std::string>& peers);
    static std::vector<std::string> getPeers();
    static void stop();

    static void syncThread(int argc, char* argv[]);
    static bool syncFromPeer(const std::string& peerUrl);
    static int pushToPeer(const std::string& peerUrl);
    static long getPeerLatestBlockId(const std::string& peerUrl);
    static long getBestPeerLatestBlockId();
    static bool isSyncedWithPeers();
    static void broadcastRecord(const CFunctions::record_structure& record);
    static void broadcastBlock(const CFunctions::block_structure& block);

private:
    static std::vector<std::string> peers;
    static bool running;
};

#endif
