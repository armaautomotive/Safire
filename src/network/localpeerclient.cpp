#include "network/localpeerclient.h"

#include "blockdb.h"
#include "functions/functions.h"
#include "networktime.h"
#include "wallet.h"
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <curl/curl.h>
#include <sstream>
#include <unistd.h>

std::vector<std::string> CLocalPeerClient::peers;
bool CLocalPeerClient::running = true;

namespace {

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string trimTrailingSlash(std::string value)
{
    while (!value.empty() && value[value.size() - 1] == '/') {
        value.erase(value.size() - 1);
    }
    return value;
}

std::string httpGet(const std::string& url)
{
    std::string readBuffer;
    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (!curl) {
        return readBuffer;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return readBuffer;
}

std::string httpPost(const std::string& url, const std::string& body)
{
    std::string readBuffer;
    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (!curl) {
        return readBuffer;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return readBuffer;
}

std::string urlEncode(const std::string& value)
{
    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    char *escaped = curl_easy_escape(curl, value.c_str(), value.length());
    std::string result = escaped ? escaped : "";
    if (escaped) {
        curl_free(escaped);
    }
    curl_easy_cleanup(curl);
    return result;
}

bool storeBlock(const CFunctions::block_structure& block)
{
    if (block.number <= 0) {
        return false;
    }

    CBlockDB blockDB;
    CFunctions::block_structure existing = blockDB.getBlockByHash(block.hash);
    if (existing.number > 0) {
        return false;
    }

    bool added = blockDB.AddBlock(block);
    if (blockDB.getFirstBlockId() == -1 && block.previous_block_id <= 0) {
        blockDB.setFirstBlockId(block.number);
    }
    if (added) {
        long repairedLatestBlockId = blockDB.rebuildBestChainIndex();
        if (repairedLatestBlockId < 0) {
            long connectedLatestBlockId = blockDB.getConnectedLatestBlockId();
            if (connectedLatestBlockId > -1) {
                blockDB.setLatestBlockId(connectedLatestBlockId);
            }
        }
    }
    return added;
}

bool storeBlocks(const std::string& blockJson)
{
    CFunctions functions;
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(blockJson);
    bool stored = false;
    for (int i = 0; i < blocks.size(); ++i) {
        stored = storeBlock(blocks.at(i)) || stored;
    }
    return stored;
}

bool submitBlockToPeer(const std::string& peer, const CFunctions::block_structure& block, std::string& response)
{
    CFunctions functions;
    std::string blockJson = functions.blockJSON(block);
    response = httpPost(peer + "/api/blocks/submit", blockJson);
    if (response.find("accepted") != std::string::npos) {
        return true;
    }

    std::string encodedBlock = urlEncode(blockJson);
    if (encodedBlock.empty()) {
        response = "unable to encode block";
        return false;
    }

    response = httpGet(peer + "/api/blocks/submit?block=" + encodedBlock);
    return response.find("accepted") != std::string::npos;
}

std::vector<CFunctions::block_structure> connectedBlocksAfter(long blockNumber)
{
    std::vector<CFunctions::block_structure> blocks;
    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    if (firstBlockId < 0) {
        return blocks;
    }

    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);

    int guard = 0;
    while (block.number > 0 && guard < 100000) {
        if (block.number > blockNumber) {
            blocks.push_back(block);
        }
        CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
        if (nextBlock.number <= 0 || nextBlock.number == block.number) {
            break;
        }
        block = nextBlock;
        guard++;
    }
    return blocks;
}

long latestBlockFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"latest_block_id\":\"") == std::string::npos) {
        return -1;
    }
    CFunctions functions;
    return functions.parseSectionLong(statusJson, "\"latest_block_id\":\"", "\"");
}

std::string latestBlockHashFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"latest_block_hash\":\"") == std::string::npos) {
        return "";
    }
    CFunctions functions;
    return functions.parseSectionString(statusJson, "\"latest_block_hash\":\"", "\"");
}

bool pullPeerCanonicalChain(const std::string& peer, long peerLatestBlockId, int maxBlocksPerSync, bool& changed)
{
    std::string response = httpGet(peer + "/api/blocks/first");
    if (response.empty()) {
        return false;
    }

    CFunctions functions;
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(response);
    if (blocks.empty()) {
        return false;
    }

    CFunctions::block_structure peerBlock = blocks.at(0);
    if (peerBlock.number <= 0 || peerBlock.hash.empty()) {
        return false;
    }

    changed = storeBlock(peerBlock) || changed;
    int received = 1;
    while (peerBlock.number > 0 && peerBlock.number < peerLatestBlockId && received < maxBlocksPerSync) {
        response = httpGet(peer + "/api/blocks/after-hash/" + peerBlock.hash);
        if (response.empty()) {
            return received > 1;
        }

        blocks = functions.parseBlockJson(response);
        if (blocks.empty()) {
            return received > 1;
        }

        peerBlock = blocks.at(0);
        if (peerBlock.number <= 0 || peerBlock.hash.empty()) {
            return received > 1;
        }

        changed = storeBlock(peerBlock) || changed;
        received++;
    }

    return received > 0;
}

}

void CLocalPeerClient::setPeers(const std::vector<std::string>& peerUrls)
{
    peers.clear();
    for (int i = 0; i < peerUrls.size(); ++i) {
        std::string peer = trimTrailingSlash(peerUrls.at(i));
        if (!peer.empty()) {
            peers.push_back(peer);
        }
    }
    running = true;
}

std::vector<std::string> CLocalPeerClient::getPeers()
{
    return peers;
}

void CLocalPeerClient::stop()
{
    running = false;
}

void CLocalPeerClient::syncThread(int argc, char* argv[])
{
    while (running) {
        syncNetworkTime();
        for (int i = 0; i < peers.size(); ++i) {
            syncFromPeer(peers.at(i));
            pushToPeer(peers.at(i));
        }
        if (running) {
            usleep(1000000 * 2);
        }
    }
}

bool CLocalPeerClient::syncFromPeer(const std::string& peerUrl)
{
    const int maxBlocksPerSync = 10000;
    bool changed = false;
    std::string peer = trimTrailingSlash(peerUrl);
    if (peer.empty()) {
        return false;
    }

    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    std::string peerStatus = httpGet(peer + "/api/status");
    long peerLatestBlockId = latestBlockFromStatus(peerStatus);
    std::string peerLatestBlockHash = latestBlockHashFromStatus(peerStatus);

    if (peerLatestBlockId < 0) {
        return false;
    }

    if (peerLatestBlockId > -1 && latestBlockId >= peerLatestBlockId) {
        CFunctions::block_structure latestBlock = blockDB.getBlock(latestBlockId);
        if (peerLatestBlockHash.length() > 0 && latestBlock.hash.compare(peerLatestBlockHash) != 0) {
            pullPeerCanonicalChain(peer, peerLatestBlockId, maxBlocksPerSync, changed);
            return changed;
        }
        return false;
    }

    if (firstBlockId == -1) {
        changed = storeBlocks(httpGet(peer + "/api/blocks/first")) || changed;
        firstBlockId = blockDB.getFirstBlockId();
        latestBlockId = blockDB.getLatestBlockId();
    }

    int received = 0;
    while (latestBlockId > -1 && received < maxBlocksPerSync) {
        if (peerLatestBlockId > -1 && latestBlockId >= peerLatestBlockId) {
            break;
        }

        long beforeLatestBlockId = latestBlockId;
        CFunctions::block_structure latestBlock = blockDB.getBlock(latestBlockId);
        std::string path = "/api/blocks/after/" + boost::lexical_cast<std::string>(latestBlockId);
        if (latestBlock.hash.length() > 0) {
            path = "/api/blocks/after-hash/" + latestBlock.hash;
        }
        std::string response = httpGet(peer + path);
        if (response.empty() && latestBlock.hash.length() > 0) {
            response = httpGet(peer + "/api/blocks/after/" + boost::lexical_cast<std::string>(latestBlockId));
        }
        if (response.empty()) {
            pullPeerCanonicalChain(peer, peerLatestBlockId, maxBlocksPerSync, changed);
            latestBlockId = blockDB.getLatestBlockId();
            if (latestBlockId > beforeLatestBlockId) {
                continue;
            }
            break;
        }

        bool sawBlock = false;
        CFunctions functions;
        std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(response);
        for (int i = 0; i < blocks.size(); ++i) {
            sawBlock = true;
            changed = storeBlock(blocks.at(i)) || changed;
        }

        if (!sawBlock) {
            break;
        }

        latestBlockId = blockDB.getLatestBlockId();
        if (latestBlockId <= beforeLatestBlockId) {
            break;
        }
        ++received;
    }

    return changed;
}

int CLocalPeerClient::pushToPeer(const std::string& peerUrl)
{
    return pushToPeerDetailed(peerUrl).pushedBlocks;
}

CLocalPeerClient::push_result CLocalPeerClient::pushToPeerDetailed(const std::string& peerUrl)
{
    const int maxBlocksPerPush = 10000;
    const long forkRepairLookbackBlocks = 100;
    push_result result;
    result.candidateBlocks = 0;
    result.pushedBlocks = 0;
    result.failedBlockId = -1;
    result.response = "";

    std::string peer = trimTrailingSlash(peerUrl);
    if (peer.empty()) {
        result.response = "empty peer URL";
        return result;
    }

    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long localLatestBlockId = blockDB.getLatestBlockId();
    long peerLatestBlockId = getPeerLatestBlockId(peer);
    long pushStartBlock = peerLatestBlockId - forkRepairLookbackBlocks;
    if (peerLatestBlockId < 0) {
        pushStartBlock = -1;
    }
    if (localLatestBlockId < 0 || firstBlockId < 0 || localLatestBlockId <= pushStartBlock) {
        return result;
    }

    std::vector<CFunctions::block_structure> blocks = connectedBlocksAfter(pushStartBlock);
    result.candidateBlocks = blocks.size();
    for (int i = 0; i < blocks.size() && result.pushedBlocks < maxBlocksPerPush; ++i) {
        std::string response;
        if (!submitBlockToPeer(peer, blocks.at(i), response)) {
            result.failedBlockId = blocks.at(i).number;
            result.response = response;
            break;
        }
        result.pushedBlocks++;
    }

    return result;
}

CLocalPeerClient::push_result CLocalPeerClient::pushFullChainToPeerDetailed(const std::string& peerUrl)
{
    const int maxBlocksPerPush = 10000;
    push_result result;
    result.candidateBlocks = 0;
    result.pushedBlocks = 0;
    result.failedBlockId = -1;
    result.response = "";

    std::string peer = trimTrailingSlash(peerUrl);
    if (peer.empty()) {
        result.response = "empty peer URL";
        return result;
    }

    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long localLatestBlockId = blockDB.getLatestBlockId();
    if (localLatestBlockId < 0 || firstBlockId < 0) {
        return result;
    }

    std::vector<CFunctions::block_structure> blocks = connectedBlocksAfter(-1);
    result.candidateBlocks = blocks.size();
    for (int i = 0; i < blocks.size() && result.pushedBlocks < maxBlocksPerPush; ++i) {
        std::string response;
        if (!submitBlockToPeer(peer, blocks.at(i), response)) {
            result.failedBlockId = blocks.at(i).number;
            result.response = response;
            break;
        }
        result.pushedBlocks++;
    }

    return result;
}

long CLocalPeerClient::getPeerLatestBlockId(const std::string& peerUrl)
{
    std::string peer = trimTrailingSlash(peerUrl);
    if (peer.empty()) {
        return -1;
    }
    return latestBlockFromStatus(httpGet(peer + "/api/status"));
}

long CLocalPeerClient::getBestPeerLatestBlockId()
{
    long bestLatestBlockId = -1;
    for (int i = 0; i < peers.size(); ++i) {
        long peerLatestBlockId = getPeerLatestBlockId(peers.at(i));
        if (peerLatestBlockId > bestLatestBlockId) {
            bestLatestBlockId = peerLatestBlockId;
        }
    }
    return bestLatestBlockId;
}

bool CLocalPeerClient::isSyncedWithPeers()
{
    long peerLatestBlockId = getBestPeerLatestBlockId();
    if (peerLatestBlockId < 0) {
        return false;
    }

    CBlockDB blockDB;
    return blockDB.getLatestBlockId() >= peerLatestBlockId;
}

bool CLocalPeerClient::syncNetworkTime()
{
    if (peers.empty()) {
        return false;
    }

    std::vector<long> offsets;
    CNetworkTime netTime;
    CFunctions functions;
    for (int i = 0; i < peers.size(); ++i) {
        long before = netTime.getLocalEpoch();
        std::string response = httpGet(peers.at(i) + "/api/time");
        long after = netTime.getLocalEpoch();
        if (response.find("\"epoch\":\"") == std::string::npos) {
            continue;
        }

        long peerEpoch = functions.parseSectionLong(response, "\"epoch\":\"", "\"");
        if (peerEpoch <= 0) {
            continue;
        }

        long localMidpoint = before + ((after - before) / 2);
        offsets.push_back(peerEpoch - localMidpoint);
    }

    if (offsets.empty()) {
        return false;
    }

    std::sort(offsets.begin(), offsets.end());
    netTime.setOffset(offsets.at(offsets.size() / 2));
    return true;
}

long CLocalPeerClient::getNetworkTimeOffset()
{
    CNetworkTime netTime;
    return netTime.getOffset();
}

void CLocalPeerClient::broadcastRecord(const CFunctions::record_structure& record)
{
    if (peers.empty()) {
        return;
    }

    CFunctions functions;
    if(functions.isRecordSizeValid(record) == false){
        return;
    }
    std::string recordJson = functions.recordJSON(record);
    for (int i = 0; i < peers.size(); ++i) {
        std::string response = httpPost(peers.at(i) + "/api/records/submit", recordJson);
        if (response.find("accepted") != std::string::npos) {
            continue;
        }

        std::string encodedRecord = urlEncode(recordJson);
        if (!encodedRecord.empty()) {
            httpGet(peers.at(i) + "/api/records/submit?record=" + encodedRecord);
        }
    }
}

void CLocalPeerClient::broadcastBlock(const CFunctions::block_structure& block)
{
    if (peers.empty()) {
        return;
    }

    CFunctions functions;
    std::string blockJson = functions.blockJSON(block);
    if(block.records.size() > CFunctions::MAX_BLOCK_RECORDS ||
       blockJson.length() > CFunctions::MAX_BLOCK_JSON_BYTES){
        return;
    }
    for(int i = 0; i < block.records.size(); i++){
        if(functions.isRecordSizeValid(block.records.at(i)) == false){
            return;
        }
    }
    for (int i = 0; i < peers.size(); ++i) {
        std::string response = httpPost(peers.at(i) + "/api/blocks/submit", blockJson);
        if (response.find("accepted") != std::string::npos) {
            continue;
        }

        std::string encodedBlock = urlEncode(blockJson);
        if (!encodedBlock.empty()) {
            httpGet(peers.at(i) + "/api/blocks/submit?block=" + encodedBlock);
        }
    }
}
