#include "network/localpeerclient.h"

#include "blockdb.h"
#include "functions/functions.h"
#include "wallet.h"
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
        }
        if (running) {
            usleep(1000000 * 2);
        }
    }
}

bool CLocalPeerClient::syncFromPeer(const std::string& peerUrl)
{
    bool changed = false;
    std::string peer = trimTrailingSlash(peerUrl);
    if (peer.empty()) {
        return false;
    }

    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();

    if (firstBlockId == -1) {
        changed = storeBlocks(httpGet(peer + "/api/blocks/first")) || changed;
        firstBlockId = blockDB.getFirstBlockId();
        latestBlockId = blockDB.getLatestBlockId();
    }

    int received = 0;
    while (latestBlockId > -1 && received < 250) {
        std::string path = "/api/blocks/after/" + boost::lexical_cast<std::string>(latestBlockId);
        std::string response = httpGet(peer + path);
        if (response.empty()) {
            break;
        }

        bool stored = storeBlocks(response);
        if (!stored) {
            break;
        }

        changed = true;
        latestBlockId = blockDB.getLatestBlockId();
        ++received;
    }

    return changed;
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
