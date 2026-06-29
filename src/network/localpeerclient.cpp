#include "network/localpeerclient.h"

#include "blockdb.h"
#include "functions/functions.h"
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
    CFunctions::block_structure existing = blockDB.getBlock(block.number);
    if (existing.number > 0) {
        return false;
    }

    blockDB.AddBlock(block);
    if (blockDB.getFirstBlockId() == -1 && block.previous_block_id <= 0) {
        blockDB.setFirstBlockId(block.number);
    }
    if (block.number > blockDB.getLatestBlockId()) {
        blockDB.setLatestBlockId(block.number);
    }
    return true;
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

bool submitBlockToPeer(const std::string& peer, const CFunctions::block_structure& block)
{
    CFunctions functions;
    std::string blockJson = functions.blockJSON(block);
    std::string encodedBlock = urlEncode(blockJson);
    if (encodedBlock.empty()) {
        return false;
    }

    std::string response = httpGet(peer + "/api/blocks/submit?block=" + encodedBlock);
    return response.find("accepted") != std::string::npos;
}

std::vector<CFunctions::block_structure> blocksAfter(long blockNumber)
{
    std::vector<CFunctions::block_structure> blocks;
    CBlockDB blockDB;
    leveldb::DB *db = blockDB.getDatabase();
    if (!db) {
        return blocks;
    }

    CFunctions functions;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        if (key.compare(0, 2, "b_") != 0) {
            continue;
        }

        std::vector<CFunctions::block_structure> parsedBlocks = functions.parseBlockJson(it->value().ToString());
        for (int i = 0; i < parsedBlocks.size(); ++i) {
            if (parsedBlocks.at(i).number > blockNumber) {
                blocks.push_back(parsedBlocks.at(i));
            }
        }
    }
    delete it;

    std::sort(blocks.begin(), blocks.end(), [](const CFunctions::block_structure& left, const CFunctions::block_structure& right) {
        return left.number < right.number;
    });
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
    long peerLatestBlockId = getPeerLatestBlockId(peer);

    if (peerLatestBlockId < 0) {
        return false;
    }

    if (peerLatestBlockId > -1 && latestBlockId >= peerLatestBlockId) {
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

        std::string path = "/api/blocks/after/" + boost::lexical_cast<std::string>(latestBlockId);
        std::string response = httpGet(peer + path);
        if (response.empty()) {
            break;
        }

        long beforeLatestBlockId = latestBlockId;
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
    const int maxBlocksPerPush = 10000;
    std::string peer = trimTrailingSlash(peerUrl);
    if (peer.empty()) {
        return 0;
    }

    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long localLatestBlockId = blockDB.getLatestBlockId();
    long peerLatestBlockId = getPeerLatestBlockId(peer);
    if (localLatestBlockId < 0 || firstBlockId < 0 || localLatestBlockId <= peerLatestBlockId) {
        return 0;
    }

    int pushed = 0;
    std::vector<CFunctions::block_structure> blocks = blocksAfter(peerLatestBlockId);
    for (int i = 0; i < blocks.size() && pushed < maxBlocksPerPush; ++i) {
        if (!submitBlockToPeer(peer, blocks.at(i))) {
            break;
        }
        pushed++;
    }

    return pushed;
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

void CLocalPeerClient::broadcastRecord(const CFunctions::record_structure& record)
{
    if (peers.empty()) {
        return;
    }

    CFunctions functions;
    std::string recordJson = functions.recordJSON(record);
    std::string encodedRecord = urlEncode(recordJson);
    if (encodedRecord.empty()) {
        return;
    }

    for (int i = 0; i < peers.size(); ++i) {
        httpGet(peers.at(i) + "/api/records/submit?record=" + encodedRecord);
    }
}

void CLocalPeerClient::broadcastBlock(const CFunctions::block_structure& block)
{
    if (peers.empty()) {
        return;
    }

    CFunctions functions;
    std::string blockJson = functions.blockJSON(block);
    std::string encodedBlock = urlEncode(blockJson);
    if (encodedBlock.empty()) {
        return;
    }

    for (int i = 0; i < peers.size(); ++i) {
        httpGet(peers.at(i) + "/api/blocks/submit?block=" + encodedBlock);
    }
}
