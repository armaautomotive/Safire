#include "network/localpeerclient.h"

#include "blockdb.h"
#include "ecdsacrypto.h"
#include "functions/functions.h"
#include "functions/ledgerstate.h"
#include "functions/selector.h"
#include "networkconfig.h"
#include "networktime.h"
#include "wallet.h"
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <curl/curl.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <unistd.h>

std::vector<std::string> CLocalPeerClient::peers;
std::map<std::string, CLocalPeerClient::peer_status> CLocalPeerClient::peerStatuses;
std::set<std::string> CLocalPeerClient::configuredPeers;
std::string CLocalPeerClient::advertisedPeerUrl = "";
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

std::string trim(const std::string& value)
{
    std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

bool looksLikePeerUrl(const std::string& value)
{
    return value.find("http://") == 0 || value.find("https://") == 0;
}

std::string normalizePeerUrl(const std::string& peerUrl)
{
    std::string peer = trimTrailingSlash(trim(peerUrl));
    if (!looksLikePeerUrl(peer)) {
        return "";
    }
    return peer;
}

std::string peerCachePath()
{
    return "peers.dat";
}

const long PEER_PURGE_SECONDS = 3L * 24L * 60L * 60L;
const int MAX_BLOCKS_PER_SYNC_BATCH = 1000;
const int MAX_BLOCKS_PER_SYNC_REQUEST = 250;
const int MAX_BLOCKS_PER_PUSH_BATCH = 200;
const int MAX_KNOWN_PEERS = 64;
const int MAX_ACTIVE_SYNC_PEERS = 12;
const long ANNOUNCE_INTERVAL_SECONDS = 60;
const long PEER_STATUS_REFRESH_SECONDS = 10;

CLocalPeerClient::peer_status emptyPeerStatus(const std::string& peer)
{
    CLocalPeerClient::peer_status status;
    status.url = peer;
    status.publicKey = "";
    status.publicName = "";
    status.firstBlockId = -1;
    status.firstBlockHash = "";
    status.latestBlockId = -1;
    status.latestBlockHash = "";
    status.protocolVersion = 0;
    status.score = 0;
    status.successes = 0;
    status.failures = 0;
    status.lastSeenEpoch = 0;
    status.lastSuccessEpoch = 0;
    status.firstFailureEpoch = 0;
    status.genesisMatch = false;
    status.reachable = false;
    status.lastError = "not checked";
    return status;
}

std::vector<std::string> split(const std::string& value, char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

long parseLongOrZero(const std::string& value)
{
    if (value.empty()) {
        return 0;
    }
    return std::atol(value.c_str());
}

int protocolVersionFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"protocol_version\":\"") == std::string::npos) {
        return 0;
    }
    CFunctions functions;
    return functions.parseSectionLong(statusJson, "\"protocol_version\":\"", "\"");
}

bool genesisMatchFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"genesis_match\":\"yes\"") != std::string::npos) {
        return true;
    }
    return false;
}

std::vector<std::string> parsePeerUrlsFromJson(const std::string& json)
{
    std::vector<std::string> urls;
    std::size_t offset = 0;
    while (true) {
        std::size_t start = json.find("\"url\":\"", offset);
        if (start == std::string::npos) {
            break;
        }
        start += 7;
        std::size_t end = json.find("\"", start);
        if (end == std::string::npos) {
            break;
        }
        std::string url = normalizePeerUrl(json.substr(start, end - start));
        if (!url.empty()) {
            urls.push_back(url);
        }
        offset = end + 1;
    }
    return urls;
}

void writePeerCache(
    const std::vector<std::string>& peerUrls,
    const std::map<std::string, CLocalPeerClient::peer_status>& peerStatuses,
    const std::set<std::string>& configuredPeers)
{
    std::ofstream outfile(peerCachePath().c_str());
    if (!outfile.good()) {
        return;
    }
    for (int i = 0; i < peerUrls.size(); ++i) {
        std::string peer = peerUrls.at(i);
        if (configuredPeers.find(peer) != configuredPeers.end()) {
            continue;
        }

        CLocalPeerClient::peer_status status = emptyPeerStatus(peer);
        std::map<std::string, CLocalPeerClient::peer_status>::const_iterator found = peerStatuses.find(peer);
        if (found != peerStatuses.end()) {
            status = found->second;
        }
        outfile << peer << "\t"
            << status.lastSuccessEpoch << "\t"
            << status.firstFailureEpoch << "\t"
            << status.score << "\n";
    }
    outfile.close();
}

std::vector<CLocalPeerClient::peer_status> loadPeerCache()
{
    std::vector<CLocalPeerClient::peer_status> cachedPeers;
    std::ifstream infile(peerCachePath().c_str());
    std::string line;
    while (std::getline(infile, line)) {
        std::vector<std::string> parts = split(line, '\t');
        if (parts.empty()) {
            continue;
        }

        std::string peer = normalizePeerUrl(parts.at(0));
        if (!peer.empty()) {
            CLocalPeerClient::peer_status status = emptyPeerStatus(peer);
            if (parts.size() > 1) {
                status.lastSuccessEpoch = parseLongOrZero(parts.at(1));
            }
            if (parts.size() > 2) {
                status.firstFailureEpoch = parseLongOrZero(parts.at(2));
            }
            if (parts.size() > 3) {
                status.score = std::atoi(parts.at(3).c_str());
            }
            cachedPeers.push_back(status);
        }
    }
    return cachedPeers;
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
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
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
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
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

std::string sha256String(const std::string& value)
{
    CECDSACrypto ecdsa;
    char hash[65];
    ecdsa.sha256((char*)value.c_str(), hash);
    return std::string(hash);
}

std::string handoffMemberSetHash(const std::vector<std::string>& activeMemberKeys)
{
    std::stringstream ss;
    for (int i = 0; i < activeMemberKeys.size(); ++i) {
        if (i > 0) {
            ss << ",";
        }
        ss << activeMemberKeys.at(i);
    }
    return sha256String(ss.str());
}

std::string handoffHashSeed(
    long blockNumber,
    const std::string& blockHash,
    const std::string& parentHash,
    const std::string& creatorKey,
    long nextSlot,
    const std::string& nextCreator,
    const std::string& activeMemberSetHash)
{
    std::stringstream seed;
    seed << blockNumber << "|"
         << blockHash << "|"
         << parentHash << "|"
         << creatorKey << "|"
         << nextSlot << "|"
         << nextCreator << "|"
         << activeMemberSetHash;
    return sha256String(seed.str());
}

std::string handoffJson(const CFunctions::block_structure& block, const std::string& creatorPrivateKey)
{
    if (block.number <= 0 || block.hash.length() == 0 || creatorPrivateKey.length() == 0) {
        return "";
    }

    CBlockDB blockDB;
    long nextSlot = block.number + 1;
    long selectionBoundary = CSelector::getSelectionBoundaryBlock(nextSlot, blockDB.getFirstBlockId());
    CLedgerState::state ledgerState = CLedgerState::build(blockDB, "", selectionBoundary);
    std::vector<std::string> activeMemberKeys = CLedgerState::activeMemberKeysAt(ledgerState, ledgerState.latest_block_id);
    std::string nextCreator = CSelector::getSelectedUserForBlock(nextSlot, ledgerState.latest_block.hash, activeMemberKeys);
    std::string activeMemberSetHash = handoffMemberSetHash(activeMemberKeys);
    std::string handoffHash = handoffHashSeed(
        block.number,
        block.hash,
        block.previous_block_hash,
        block.creator_key,
        nextSlot,
        nextCreator,
        activeMemberSetHash);

    CECDSACrypto ecdsa;
    std::string signature;
    ecdsa.SignMessage(creatorPrivateKey, handoffHash, signature);

    std::stringstream ss;
    ss << "{\"type\":\"BLOCK_HANDOFF\",";
    ss << "\"block_number\":\"" << block.number << "\",";
    ss << "\"block_hash\":\"" << block.hash << "\",";
    ss << "\"parent_hash\":\"" << block.previous_block_hash << "\",";
    ss << "\"creator\":\"" << block.creator_key << "\",";
    ss << "\"next_slot\":\"" << nextSlot << "\",";
    ss << "\"next_creator\":\"" << nextCreator << "\",";
    ss << "\"active_member_set_hash\":\"" << activeMemberSetHash << "\",";
    ss << "\"handoff_hash\":\"" << handoffHash << "\",";
    ss << "\"signature\":\"" << signature << "\"}";
    return ss.str();
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
    if (added && blockDB.getFirstBlockId() == -1 && block.previous_block_id <= 0) {
        blockDB.setFirstBlockId(block.number);
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

int storeParsedBlocks(const std::vector<CFunctions::block_structure>& blocks, bool& changed)
{
    int storedCount = 0;
    for (int i = 0; i < blocks.size(); ++i) {
        if (blocks.at(i).number <= 0 || blocks.at(i).hash.empty()) {
            continue;
        }
        bool stored = storeBlock(blocks.at(i));
        changed = stored || changed;
        if (stored) {
            ++storedCount;
        }
    }
    return storedCount;
}

bool submitBlockToPeer(const std::string& peer, const CFunctions::block_structure& block, std::string& response)
{
    CFunctions functions;
    std::string blockJson = functions.blockJSON(block);
    response = httpPost(peer + "/api/blocks/submit", blockJson);
    if (response.find("\"status\":\"accepted\"") != std::string::npos || response == "accepted") {
        return true;
    }

    std::string encodedBlock = urlEncode(blockJson);
    if (encodedBlock.empty()) {
        response = "unable to encode block";
        return false;
    }

    response = httpGet(peer + "/api/blocks/submit?block=" + encodedBlock);
    return response.find("\"status\":\"accepted\"") != std::string::npos || response == "accepted";
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

long firstBlockFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"first_block_id\":\"") == std::string::npos) {
        return -1;
    }
    CFunctions functions;
    return functions.parseSectionLong(statusJson, "\"first_block_id\":\"", "\"");
}

std::string firstBlockHashFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"first_block_hash\":\"") == std::string::npos) {
        return "";
    }
    CFunctions functions;
    return functions.parseSectionString(statusJson, "\"first_block_hash\":\"", "\"");
}

std::string latestBlockHashFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"latest_block_hash\":\"") == std::string::npos) {
        return "";
    }
    CFunctions functions;
    return functions.parseSectionString(statusJson, "\"latest_block_hash\":\"", "\"");
}

std::string publicKeyFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"public_key\":\"") == std::string::npos) {
        return "";
    }
    CFunctions functions;
    return functions.parseSectionString(statusJson, "\"public_key\":\"", "\"");
}

std::string publicNameFromStatus(const std::string& statusJson)
{
    if (statusJson.find("\"public_name\":\"") == std::string::npos) {
        return "";
    }
    CFunctions functions;
    return functions.parseSectionString(statusJson, "\"public_name\":\"", "\"");
}

CLocalPeerClient::peer_status statusFromPeer(const std::string& peer)
{
    CLocalPeerClient::peer_status status = emptyPeerStatus(peer);
    CNetworkTime netTime;
    long now = netTime.getLocalEpoch();
    std::string peerStatus = httpGet(peer + "/api/status");
    if (peerStatus.empty()) {
        status.score = -5;
        status.firstFailureEpoch = now;
        status.lastError = "empty status response";
        return status;
    }

    status.firstBlockId = firstBlockFromStatus(peerStatus);
    status.firstBlockHash = firstBlockHashFromStatus(peerStatus);
    status.latestBlockId = latestBlockFromStatus(peerStatus);
    status.latestBlockHash = latestBlockHashFromStatus(peerStatus);
    status.publicKey = publicKeyFromStatus(peerStatus);
    status.publicName = publicNameFromStatus(peerStatus);
    status.protocolVersion = protocolVersionFromStatus(peerStatus);
    CNetworkConfig config = CNetworkConfig::load();
    status.genesisMatch = config.genesisMatches(status.firstBlockId, status.firstBlockHash) &&
        genesisMatchFromStatus(peerStatus);
    status.reachable = status.latestBlockId >= 0;
    status.lastSeenEpoch = now;

    if (!status.reachable) {
        status.score = -5;
        status.firstFailureEpoch = now;
        status.lastError = "missing latest block";
    } else if (!status.genesisMatch) {
        status.score = -20;
        status.firstFailureEpoch = now;
        status.lastError = "genesis mismatch";
    } else {
        status.score = 10;
        status.lastSuccessEpoch = now;
        status.firstFailureEpoch = 0;
        status.lastError = "";
    }
    return status;
}

void mergePeerStatus(CLocalPeerClient::peer_status& current, const CLocalPeerClient::peer_status& update)
{
    int previousScore = current.score;
    int previousSuccesses = current.successes;
    int previousFailures = current.failures;
    long previousLastSuccessEpoch = current.lastSuccessEpoch;
    long previousFirstFailureEpoch = current.firstFailureEpoch;
    current = update;
    current.successes += previousSuccesses;
    current.failures += previousFailures;
    if (previousLastSuccessEpoch > current.lastSuccessEpoch) {
        current.lastSuccessEpoch = previousLastSuccessEpoch;
    }

    if (update.reachable && update.genesisMatch) {
        current.successes += 1;
        current.score = std::min(100, previousScore + 10);
        current.firstFailureEpoch = 0;
        if (update.lastSuccessEpoch > 0) {
            current.lastSuccessEpoch = update.lastSuccessEpoch;
        }
        current.lastError = "";
    } else {
        current.failures += 1;
        current.score = std::max(-100, previousScore - 15);
        if (previousFirstFailureEpoch > 0) {
            current.firstFailureEpoch = previousFirstFailureEpoch;
        } else if (update.firstFailureEpoch > 0) {
            current.firstFailureEpoch = update.firstFailureEpoch;
        }
        if (current.lastError.empty()) {
            current.lastError = "peer check failed";
        }
    }
}

bool pullPeerCanonicalChain(const std::string& peer, long peerLatestBlockId, int maxBlocksPerSync, bool& changed)
{
    CBlockDB blockDB;
    long latestBeforeRepair = blockDB.getLatestBlockId();
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
    CNetworkConfig config = CNetworkConfig::load();
    if(config.genesisMatches(peerBlock.number, peerBlock.hash) == false){
        return false;
    }

    changed = storeBlock(peerBlock) || changed;
    int received = 1;
    while (peerBlock.number > 0 && peerBlock.number < peerLatestBlockId && received < maxBlocksPerSync) {
        int requestLimit = std::min(MAX_BLOCKS_PER_SYNC_REQUEST, maxBlocksPerSync - received);
        response = httpGet(peer + "/api/blocks/batch-after-hash/" + peerBlock.hash + "/" + boost::lexical_cast<std::string>(requestLimit));
        if (response.empty()) {
            response = httpGet(peer + "/api/blocks/after-hash/" + peerBlock.hash);
        }
        if (response.empty()) {
            long repairedLatestBlockId = blockDB.rebuildBestChainIndex();
            if (repairedLatestBlockId > latestBeforeRepair) {
                changed = true;
            }
            return received > 1;
        }

        blocks = functions.parseBlockJson(response);
        if (blocks.empty()) {
            long repairedLatestBlockId = blockDB.rebuildBestChainIndex();
            if (repairedLatestBlockId > latestBeforeRepair) {
                changed = true;
            }
            return received > 1;
        }

        int storedCount = 0;
        for (int i = 0; i < blocks.size() && received < maxBlocksPerSync; ++i) {
            peerBlock = blocks.at(i);
            if (peerBlock.number <= 0 || peerBlock.hash.empty()) {
                break;
            }
            changed = storeBlock(peerBlock) || changed;
            ++storedCount;
            ++received;
        }

        if (storedCount == 0 || peerBlock.number <= 0 || peerBlock.hash.empty()) {
            long repairedLatestBlockId = blockDB.rebuildBestChainIndex();
            if (repairedLatestBlockId > latestBeforeRepair) {
                changed = true;
            }
            return received > 1;
        }
    }

    long repairedLatestBlockId = blockDB.rebuildBestChainIndex();
    if (repairedLatestBlockId > latestBeforeRepair) {
        changed = true;
    }
    return received > 0;
}

bool pullPeerBlocksByNumberRange(const std::string& peer, long startBlockId, long peerLatestBlockId, int maxBlocksPerSync, bool& changed)
{
    if (peerLatestBlockId < 0) {
        return false;
    }
    if (startBlockId < 0) {
        startBlockId = 0;
    }

    CBlockDB blockDB;
    long latestBeforeRepair = blockDB.getLatestBlockId();
    CFunctions functions;
    int received = 0;
    for (long blockId = startBlockId; blockId <= peerLatestBlockId && received < maxBlocksPerSync; ++blockId) {
        std::string response = httpGet(peer + "/api/blocks/" + boost::lexical_cast<std::string>(blockId));
        if (response.empty()) {
            continue;
        }

        std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(response);
        for (int i = 0; i < blocks.size(); ++i) {
            if (blocks.at(i).number > 0) {
                changed = storeBlock(blocks.at(i)) || changed;
                ++received;
            }
        }
    }

    long repairedLatestBlockId = blockDB.rebuildBestChainIndex();
    if (repairedLatestBlockId > latestBeforeRepair) {
        changed = true;
    }

    return received > 0;
}

bool pullPeerForkRepairWindow(const std::string& peer,
                              long firstBlockId,
                              long localLatestBlockId,
                              long peerLatestBlockId,
                              int maxBlocksPerSync,
                              bool& changed)
{
    if (peerLatestBlockId < 0 || firstBlockId < 0) {
        return false;
    }

    const long forkRepairLookbackSlots = 60;
    long startBlockId = localLatestBlockId - forkRepairLookbackSlots;
    if (startBlockId < firstBlockId) {
        startBlockId = firstBlockId;
    }

    return pullPeerBlocksByNumberRange(peer, startBlockId, peerLatestBlockId, maxBlocksPerSync, changed);
}

}

void CLocalPeerClient::setPeers(const std::vector<std::string>& peerUrls)
{
    peers.clear();
    peerStatuses.clear();
    configuredPeers.clear();
    for (int i = 0; i < peerUrls.size(); ++i) {
        std::string peer = normalizePeerUrl(peerUrls.at(i));
        if (!peer.empty() && peerStatuses.find(peer) == peerStatuses.end()) {
            peers.push_back(peer);
            peerStatuses[peer] = emptyPeerStatus(peer);
            configuredPeers.insert(peer);
        }
    }

    if (peerUrls.size() > 0) {
        std::vector<peer_status> cachedPeers = loadPeerCache();
        for (int i = 0; i < cachedPeers.size(); ++i) {
            std::string peer = cachedPeers.at(i).url;
            if (addPeer(peer, false)) {
                peerStatuses[peer] = cachedPeers.at(i);
            }
        }
        purgeUnavailablePeers();
    }
    running = true;
}

void CLocalPeerClient::setAdvertisedPeer(const std::string& peerUrl)
{
    advertisedPeerUrl = normalizePeerUrl(peerUrl);
}

std::string CLocalPeerClient::getAdvertisedPeer()
{
    return advertisedPeerUrl;
}

bool CLocalPeerClient::addPeer(const std::string& peerUrl, bool persist)
{
    std::string peer = normalizePeerUrl(peerUrl);
    if (peer.empty()) {
        return false;
    }
    if (!advertisedPeerUrl.empty() && peer.compare(advertisedPeerUrl) == 0) {
        return false;
    }
    if (peerStatuses.find(peer) != peerStatuses.end()) {
        return false;
    }
    if (peers.size() >= MAX_KNOWN_PEERS) {
        return false;
    }

    peers.push_back(peer);
    peerStatuses[peer] = emptyPeerStatus(peer);
    if (persist) {
        savePeerCache();
    }
    return true;
}

bool CLocalPeerClient::addVerifiedPeer(const std::string& peerUrl, bool persist)
{
    std::string peer = normalizePeerUrl(peerUrl);
    if (peer.empty()) {
        return false;
    }
    if (!advertisedPeerUrl.empty() && peer.compare(advertisedPeerUrl) == 0) {
        return false;
    }
    if (peerStatuses.find(peer) != peerStatuses.end()) {
        return false;
    }

    peer_status status = statusFromPeer(peer);
    if (status.reachable == false || status.genesisMatch == false) {
        return false;
    }

    if (addPeer(peer, false) == false) {
        return false;
    }
    peerStatuses[peer] = status;
    if (persist) {
        savePeerCache();
    }
    return true;
}

void CLocalPeerClient::savePeerCache()
{
    writePeerCache(peers, peerStatuses, configuredPeers);
}

void CLocalPeerClient::purgeUnavailablePeers()
{
    CNetworkTime netTime;
    long now = netTime.getLocalEpoch();
    bool changed = false;
    std::vector<std::string> keptPeers;
    for (int i = 0; i < peers.size(); ++i) {
        std::string peer = peers.at(i);
        bool configuredPeer = configuredPeers.find(peer) != configuredPeers.end();
        peer_status status = emptyPeerStatus(peer);
        if (peerStatuses.find(peer) != peerStatuses.end()) {
            status = peerStatuses[peer];
        }

        bool expired = !configuredPeer &&
            status.firstFailureEpoch > 0 &&
            now - status.firstFailureEpoch >= PEER_PURGE_SECONDS &&
            (status.lastSuccessEpoch == 0 || status.lastSuccessEpoch < status.firstFailureEpoch);

        if (expired) {
            peerStatuses.erase(peer);
            changed = true;
            continue;
        }
        keptPeers.push_back(peer);
    }

    if (changed) {
        peers = keptPeers;
        savePeerCache();
    }
}

std::vector<std::string> CLocalPeerClient::getPeers()
{
    return peers;
}

std::vector<CLocalPeerClient::peer_status> CLocalPeerClient::getPeerStatuses(bool refresh)
{
    std::vector<peer_status> statuses;
    bool changed = false;
    for (int i = 0; i < peers.size(); ++i) {
        std::string peer = peers.at(i);
        if (peerStatuses.find(peer) == peerStatuses.end()) {
            peerStatuses[peer] = emptyPeerStatus(peer);
        }
        if (refresh) {
            peer_status status = statusFromPeer(peer);
            mergePeerStatus(peerStatuses[peer], status);
            changed = true;
        }
        statuses.push_back(peerStatuses[peer]);
    }
    if (changed) {
        savePeerCache();
    }
    return statuses;
}

void CLocalPeerClient::discoverPeers()
{
    if (!running) {
        return;
    }
    std::vector<std::string> currentPeers = peers;
    for (int i = 0; i < currentPeers.size(); ++i) {
        if (!running) {
            break;
        }
        std::string peer = currentPeers.at(i);
        std::string response = httpGet(peer + "/api/peers");
        if (response.empty()) {
            continue;
        }

        std::vector<std::string> discoveredPeers = parsePeerUrlsFromJson(response);
        for (int j = 0; j < discoveredPeers.size(); ++j) {
            if (!running) {
                break;
            }
            std::string discoveredPeer = discoveredPeers.at(j);
            if (discoveredPeer.compare(peer) == 0) {
                continue;
            }
            if (peerStatuses.find(discoveredPeer) != peerStatuses.end()) {
                continue;
            }

            addVerifiedPeer(discoveredPeer, true);
        }
    }
    purgeUnavailablePeers();
}

void CLocalPeerClient::announceToPeers()
{
    if (!running || advertisedPeerUrl.empty() || peers.empty()) {
        return;
    }

    std::stringstream body;
    body << "{\"url\":\"" << advertisedPeerUrl << "\"}";
    std::vector<std::string> currentPeers = peers;
    for (int i = 0; i < currentPeers.size(); ++i) {
        if (!running) {
            break;
        }
        std::string peer = normalizePeerUrl(currentPeers.at(i));
        if (peer.empty() || peer.compare(advertisedPeerUrl) == 0) {
            continue;
        }
        if (peerStatuses.find(peer) != peerStatuses.end() && shouldSkipPeerForScore(peerStatuses[peer])) {
            continue;
        }
        std::string response = httpPost(peer + "/api/peers/announce", body.str());
        if (response.find("\"status\":\"ok\"") == std::string::npos &&
            response.find("\"status\":\"known\"") == std::string::npos) {
            std::string encodedPeer = urlEncode(advertisedPeerUrl);
            if (!encodedPeer.empty()) {
                httpGet(peer + "/api/peers/announce?url=" + encodedPeer);
            }
        }
    }
}

void CLocalPeerClient::stop()
{
    running = false;
}

bool CLocalPeerClient::shouldSkipPeerForScore(const peer_status& status)
{
    if (status.score >= -40) {
        return false;
    }

    std::string peer = normalizePeerUrl(status.url);
    return configuredPeers.find(peer) == configuredPeers.end();
}

void CLocalPeerClient::syncThread(int argc, char* argv[])
{
    CNetworkTime netTime;
    long lastAnnounceEpoch = 0;
    while (running) {
        syncNetworkTime();
        discoverPeers();
        long now = netTime.getLocalEpoch();
        if (lastAnnounceEpoch == 0 || now - lastAnnounceEpoch >= ANNOUNCE_INTERVAL_SECONDS) {
            announceToPeers();
            lastAnnounceEpoch = now;
        }
        purgeUnavailablePeers();
        std::vector<peer_status> statuses = getPeerStatuses();
        std::vector<std::string> priorityPeers = getPriorityPeersForUpcomingCreators(20);
        std::set<std::string> priorityPeerSet(priorityPeers.begin(), priorityPeers.end());
        std::sort(statuses.begin(), statuses.end(),
            [priorityPeerSet](const peer_status& a, const peer_status& b){
                bool aPriority = priorityPeerSet.find(a.url) != priorityPeerSet.end();
                bool bPriority = priorityPeerSet.find(b.url) != priorityPeerSet.end();
                if (aPriority != bPriority) {
                    return aPriority;
                }
                if (a.score == b.score) {
                    return a.latestBlockId > b.latestBlockId;
                }
                return a.score > b.score;
            });
        int activePeers = 0;
        for (int i = 0; i < statuses.size(); ++i) {
            if (shouldSkipPeerForScore(statuses.at(i))) {
                continue;
            }
            if (activePeers >= MAX_ACTIVE_SYNC_PEERS) {
                break;
            }
            syncFromPeer(statuses.at(i).url);
            pushToPeer(statuses.at(i).url);
            activePeers++;
        }
        if (running) {
            usleep(1000000 * 2);
        }
    }
}

bool CLocalPeerClient::syncFromPeer(const std::string& peerUrl)
{
    const int maxBlocksPerSync = MAX_BLOCKS_PER_SYNC_BATCH;
    bool changed = false;
    if (!running) {
        return false;
    }
    std::string peer = normalizePeerUrl(peerUrl);
    if (peer.empty()) {
        return false;
    }

    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    if (firstBlockId < 0 && latestBlockId >= 0) {
        blockDB.DeleteAll();
        firstBlockId = -1;
        latestBlockId = -1;
    }
    std::string peerStatus = httpGet(peer + "/api/status");
    long peerFirstBlockId = firstBlockFromStatus(peerStatus);
    std::string peerFirstBlockHash = firstBlockHashFromStatus(peerStatus);
    long peerLatestBlockId = latestBlockFromStatus(peerStatus);
    std::string peerLatestBlockHash = latestBlockHashFromStatus(peerStatus);
    CNetworkConfig config = CNetworkConfig::load();
    peer_status status = statusFromPeer(peer);
    if (peerStatuses.find(peer) == peerStatuses.end()) {
        peerStatuses[peer] = emptyPeerStatus(peer);
    }
    mergePeerStatus(peerStatuses[peer], status);
    savePeerCache();

    if (peerLatestBlockId < 0) {
        peerStatuses[peer].lastError = "missing latest block";
        savePeerCache();
        return false;
    }
    if(config.genesisMatches(peerFirstBlockId, peerFirstBlockHash) == false){
        peerStatuses[peer].lastError = "genesis mismatch";
        savePeerCache();
        return false;
    }
    if(firstBlockId > 0){
        CFunctions::block_structure firstBlock = blockDB.getBlock(firstBlockId);
        if (firstBlock.number <= 0 || firstBlock.hash.length() == 0) {
            pullPeerCanonicalChain(peer, peerLatestBlockId, maxBlocksPerSync, changed);
            firstBlockId = blockDB.getFirstBlockId();
            latestBlockId = blockDB.getLatestBlockId();
            firstBlock = blockDB.getBlock(firstBlockId);
        }
        if(config.genesisMatches(firstBlockId, firstBlock.hash) == false){
            peerStatuses[peer].lastError = "local genesis mismatch";
            savePeerCache();
            return false;
        }
    }

    long repairedLatestBlockId = blockDB.rebuildBestChainIndex();
    if (repairedLatestBlockId > latestBlockId) {
        changed = true;
        latestBlockId = repairedLatestBlockId;
    }

    if (peerLatestBlockId > -1 && latestBlockId >= peerLatestBlockId) {
        CFunctions::block_structure latestBlock = blockDB.getBlock(latestBlockId);
        if (peerLatestBlockHash.length() > 0 && latestBlock.hash.compare(peerLatestBlockHash) != 0) {
            long beforeRepairLatestBlockId = blockDB.getLatestBlockId();
            bool pulled = pullPeerForkRepairWindow(peer, firstBlockId, latestBlockId, peerLatestBlockId, maxBlocksPerSync, changed);
            long afterForkPullLatestBlockId = blockDB.getLatestBlockId();
            if (pulled == false || afterForkPullLatestBlockId <= beforeRepairLatestBlockId) {
                pullPeerCanonicalChain(peer, peerLatestBlockId, maxBlocksPerSync, changed);
            }
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
        if (!running) {
            break;
        }
        if (peerLatestBlockId > -1 && latestBlockId >= peerLatestBlockId) {
            break;
        }

        long beforeLatestBlockId = latestBlockId;
        CFunctions::block_structure latestBlock = blockDB.getBlock(latestBlockId);
        int requestLimit = std::min(MAX_BLOCKS_PER_SYNC_REQUEST, maxBlocksPerSync - received);
        std::string path = "/api/blocks/after/" + boost::lexical_cast<std::string>(latestBlockId);
        if (latestBlock.hash.length() > 0) {
            path = "/api/blocks/batch-after-hash/" + latestBlock.hash + "/" + boost::lexical_cast<std::string>(requestLimit);
        }
        std::string response = httpGet(peer + path);
        if (response.empty() && latestBlock.hash.length() > 0) {
            response = httpGet(peer + "/api/blocks/after-hash/" + latestBlock.hash);
        }
        if (response.empty() && latestBlock.hash.length() > 0) {
            response = httpGet(peer + "/api/blocks/after/" + boost::lexical_cast<std::string>(latestBlockId));
        }
        if (response.empty()) {
            bool pulled = pullPeerForkRepairWindow(peer, firstBlockId, beforeLatestBlockId, peerLatestBlockId, maxBlocksPerSync, changed);
            latestBlockId = blockDB.getLatestBlockId();
            if (pulled == false || latestBlockId <= beforeLatestBlockId) {
                pullPeerCanonicalChain(peer, peerLatestBlockId, maxBlocksPerSync, changed);
            }
            latestBlockId = blockDB.getLatestBlockId();
            if (latestBlockId > beforeLatestBlockId) {
                continue;
            }
            break;
        }

        CFunctions functions;
        std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(response);
        int sawBlocks = storeParsedBlocks(blocks, changed);

        if (sawBlocks <= 0) {
            bool pulled = pullPeerForkRepairWindow(peer, firstBlockId, beforeLatestBlockId, peerLatestBlockId, maxBlocksPerSync, changed);
            latestBlockId = blockDB.getLatestBlockId();
            if (pulled == false || latestBlockId <= beforeLatestBlockId) {
                break;
            }
            continue;
        }

        latestBlockId = blockDB.getLatestBlockId();
        if (latestBlockId <= beforeLatestBlockId) {
            long repairedLatestBlockId = blockDB.rebuildBestChainIndex();
            if (repairedLatestBlockId > latestBlockId) {
                changed = true;
                latestBlockId = repairedLatestBlockId;
            }
        }
        if (latestBlockId <= beforeLatestBlockId) {
            break;
        }
        received += sawBlocks;
    }

    return changed;
}

int CLocalPeerClient::pushToPeer(const std::string& peerUrl)
{
    return pushToPeerDetailed(peerUrl).pushedBlocks;
}

CLocalPeerClient::push_result CLocalPeerClient::pushToPeerDetailed(const std::string& peerUrl)
{
    const int maxBlocksPerPush = MAX_BLOCKS_PER_PUSH_BATCH;
    const long forkRepairLookbackBlocks = 100;
    push_result result;
    result.candidateBlocks = 0;
    result.pushedBlocks = 0;
    result.failedBlockId = -1;
    result.response = "";

    if (!running) {
        result.response = "stopping";
        return result;
    }

    std::string peer = normalizePeerUrl(peerUrl);
    if (peer.empty()) {
        result.response = "empty peer URL";
        return result;
    }

    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    long localLatestBlockId = blockDB.getLatestBlockId();
    peer_status status = statusFromPeer(peer);
    if (peerStatuses.find(peer) == peerStatuses.end()) {
        peerStatuses[peer] = emptyPeerStatus(peer);
    }
    mergePeerStatus(peerStatuses[peer], status);
    savePeerCache();

    long peerLatestBlockId = status.genesisMatch && status.reachable ? status.latestBlockId : -1;
    long pushStartBlock = peerLatestBlockId - forkRepairLookbackBlocks;
    if (peerLatestBlockId < 0) {
        pushStartBlock = -1;
    } else if (status.latestBlockHash.length() > 0) {
        CFunctions::block_structure localPeerTip = blockDB.getBlock(peerLatestBlockId);
        if (localPeerTip.number <= 0 || localPeerTip.hash.compare(status.latestBlockHash) != 0) {
            pushStartBlock = firstBlockId - 1;
        }
    }
    if (localLatestBlockId < 0 || firstBlockId < 0 || localLatestBlockId <= pushStartBlock) {
        return result;
    }

    std::vector<CFunctions::block_structure> blocks = connectedBlocksAfter(pushStartBlock);
    result.candidateBlocks = blocks.size();
    for (int i = 0; i < blocks.size() && result.pushedBlocks < maxBlocksPerPush; ++i) {
        if (!running) {
            result.response = "stopping";
            break;
        }
        std::string response;
        if (!submitBlockToPeer(peer, blocks.at(i), response)) {
            result.failedBlockId = blocks.at(i).number;
            result.response = response;
            if (peerStatuses.find(peer) == peerStatuses.end()) {
                peerStatuses[peer] = emptyPeerStatus(peer);
            }
            peerStatuses[peer].failures += 1;
            CNetworkTime netTime;
            if (peerStatuses[peer].firstFailureEpoch == 0) {
                peerStatuses[peer].firstFailureEpoch = netTime.getLocalEpoch();
            }
            peerStatuses[peer].score = std::max(-100, peerStatuses[peer].score - 10);
            peerStatuses[peer].lastError = response.empty() ? "push failed" : response;
            savePeerCache();
            break;
        }
        if (peerStatuses.find(peer) == peerStatuses.end()) {
            peerStatuses[peer] = emptyPeerStatus(peer);
        }
        peerStatuses[peer].successes += 1;
        CNetworkTime netTime;
        peerStatuses[peer].lastSuccessEpoch = netTime.getLocalEpoch();
        peerStatuses[peer].firstFailureEpoch = 0;
        peerStatuses[peer].score = std::min(100, peerStatuses[peer].score + 1);
        savePeerCache();
        result.pushedBlocks++;
    }

    return result;
}

CLocalPeerClient::push_result CLocalPeerClient::pushFullChainToPeerDetailed(const std::string& peerUrl)
{
    const int maxBlocksPerPush = MAX_BLOCKS_PER_PUSH_BATCH;
    push_result result;
    result.candidateBlocks = 0;
    result.pushedBlocks = 0;
    result.failedBlockId = -1;
    result.response = "";

    if (!running) {
        result.response = "stopping";
        return result;
    }

    std::string peer = normalizePeerUrl(peerUrl);
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
        if (!running) {
            result.response = "stopping";
            break;
        }
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
    if (!running) {
        return -1;
    }
    std::string peer = normalizePeerUrl(peerUrl);
    if (peer.empty()) {
        return -1;
    }
    peer_status status = statusFromPeer(peer);
    if (peerStatuses.find(peer) == peerStatuses.end()) {
        peerStatuses[peer] = emptyPeerStatus(peer);
    }
    mergePeerStatus(peerStatuses[peer], status);
    savePeerCache();
    purgeUnavailablePeers();
    if(status.genesisMatch == false || status.reachable == false){
        return -1;
    }
    return status.latestBlockId;
}

CLocalPeerClient::peer_status CLocalPeerClient::getBestPeerStatus()
{
    CNetworkTime netTime;
    long now = netTime.getLocalEpoch();
    for (int i = 0; i < peers.size(); ++i) {
        if (peerStatuses.find(peers.at(i)) == peerStatuses.end() ||
            peerStatuses[peers.at(i)].lastSeenEpoch == 0 ||
            now - peerStatuses[peers.at(i)].lastSeenEpoch >= PEER_STATUS_REFRESH_SECONDS) {
            getPeerLatestBlockId(peers.at(i));
        }
    }

    peer_status best = emptyPeerStatus("");
    bool found = false;
    std::vector<peer_status> statuses = getPeerStatuses();
    for (int i = 0; i < statuses.size(); ++i) {
        if (statuses.at(i).reachable == false || statuses.at(i).genesisMatch == false || shouldSkipPeerForScore(statuses.at(i))) {
            continue;
        }
        if (found == false ||
            statuses.at(i).latestBlockId > best.latestBlockId ||
            (statuses.at(i).latestBlockId == best.latestBlockId && statuses.at(i).score > best.score)) {
            best = statuses.at(i);
            found = true;
        }
    }
    return best;
}

long CLocalPeerClient::getBestPeerLatestBlockId()
{
    return getBestPeerStatus().latestBlockId;
}

std::string CLocalPeerClient::getBestPeerLatestBlockHash()
{
    return getBestPeerStatus().latestBlockHash;
}

bool CLocalPeerClient::isSyncedWithPeers()
{
    peer_status bestPeer = getBestPeerStatus();
    if (bestPeer.latestBlockId < 0) {
        return false;
    }

    CBlockDB blockDB;
    if (blockDB.getLatestBlockId() < bestPeer.latestBlockId) {
        return false;
    }

    if (bestPeer.latestBlockHash.length() == 0) {
        return true;
    }

    CFunctions::block_structure localPeerTip = blockDB.getBlock(bestPeer.latestBlockId);
    return localPeerTip.number > 0 &&
        localPeerTip.hash.length() > 0 &&
        localPeerTip.hash.compare(bestPeer.latestBlockHash) == 0;
}

bool CLocalPeerClient::syncNetworkTime()
{
    if (!running || peers.empty()) {
        return false;
    }

    std::vector<long> offsets;
    CNetworkTime netTime;
    CFunctions functions;
    std::vector<peer_status> statuses = getPeerStatuses();
    for (int i = 0; i < statuses.size(); ++i) {
        if (!running) {
            break;
        }
        if (shouldSkipPeerForScore(statuses.at(i))) {
            continue;
        }
        long before = netTime.getLocalEpoch();
        std::string response = httpGet(statuses.at(i).url + "/api/time");
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

std::vector<CLocalPeerClient::creator_schedule_slot> CLocalPeerClient::getUpcomingCreatorSchedule(int slotCount)
{
    std::vector<creator_schedule_slot> schedule;
    if (slotCount <= 0) {
        return schedule;
    }
    if (slotCount > 200) {
        slotCount = 200;
    }

    CBlockDB blockDB;
    long firstBlockId = blockDB.getFirstBlockId();
    if (firstBlockId <= 0) {
        return schedule;
    }

    CNetworkTime netTime;
    long currentSlot = netTime.getEpoch() / 15;
    long cachedSelectionBoundary = -1;
    CLedgerState::state cachedSelectionState;
    for (int i = 0; i < slotCount; ++i) {
        long slot = currentSlot + i;
        long selectionBoundary = CSelector::getSelectionBoundaryBlock(slot, firstBlockId);
        if (selectionBoundary != cachedSelectionBoundary) {
            cachedSelectionState = CLedgerState::build(blockDB, "", selectionBoundary);
            cachedSelectionBoundary = selectionBoundary;
        }
        std::vector<std::string> activeMemberKeys = CLedgerState::activeMemberKeysAt(cachedSelectionState, selectionBoundary);
        std::string creatorKey = CSelector::getSelectedUserForBlock(slot, cachedSelectionState.latest_block.hash, activeMemberKeys);

        creator_schedule_slot item;
        item.blockId = slot;
        item.selectionBoundaryBlock = selectionBoundary;
        item.selectionCheckpointBlock = cachedSelectionState.latest_block.number;
        item.selectionCheckpointHash = cachedSelectionState.latest_block.hash;
        item.creatorKey = creatorKey;
        item.creatorPeerUrl = "";

        if (creatorKey.length() > 0) {
            for (std::map<std::string, peer_status>::const_iterator it = peerStatuses.begin(); it != peerStatuses.end(); ++it) {
                const peer_status& status = it->second;
                if (status.publicKey.compare(creatorKey) == 0 &&
                    status.reachable &&
                    status.genesisMatch &&
                    !shouldSkipPeerForScore(status)) {
                    item.creatorPeerUrl = status.url;
                    break;
                }
            }
        }
        schedule.push_back(item);
    }
    return schedule;
}

std::vector<std::string> CLocalPeerClient::getPriorityPeersForUpcomingCreators(int slotCount)
{
    std::vector<std::string> priorityPeers;
    std::set<std::string> seen;
    std::vector<creator_schedule_slot> schedule = getUpcomingCreatorSchedule(slotCount);
    for (int i = 0; i < schedule.size(); ++i) {
        std::string peer = normalizePeerUrl(schedule.at(i).creatorPeerUrl);
        if (peer.empty() || seen.find(peer) != seen.end()) {
            continue;
        }
        priorityPeers.push_back(peer);
        seen.insert(peer);
    }
    return priorityPeers;
}

void CLocalPeerClient::broadcastRecord(const CFunctions::record_structure& record)
{
    if (!running || peers.empty()) {
        return;
    }

    CFunctions functions;
    if(functions.isRecordSizeValid(record) == false){
        return;
    }
    std::string recordJson = functions.recordJSON(record);
    std::vector<std::string> orderedPeers = getPriorityPeersForUpcomingCreators(20);
    std::set<std::string> seenPeers(orderedPeers.begin(), orderedPeers.end());
    for (int i = 0; i < peers.size(); ++i) {
        if (seenPeers.find(peers.at(i)) == seenPeers.end()) {
            orderedPeers.push_back(peers.at(i));
            seenPeers.insert(peers.at(i));
        }
    }
    for (int i = 0; i < orderedPeers.size(); ++i) {
        if (!running) {
            break;
        }
        if (peerStatuses.find(orderedPeers.at(i)) != peerStatuses.end() && shouldSkipPeerForScore(peerStatuses[orderedPeers.at(i)])) {
            continue;
        }
        std::string response = httpPost(orderedPeers.at(i) + "/api/records/submit", recordJson);
        if (response.find("accepted") != std::string::npos) {
            continue;
        }

        std::string encodedRecord = urlEncode(recordJson);
        if (!encodedRecord.empty()) {
            httpGet(orderedPeers.at(i) + "/api/records/submit?record=" + encodedRecord);
        }
    }
}

void CLocalPeerClient::broadcastBlock(const CFunctions::block_structure& block)
{
    if (!running || peers.empty()) {
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
    std::vector<std::string> orderedPeers = getPriorityPeersForUpcomingCreators(20);
    std::set<std::string> seenPeers(orderedPeers.begin(), orderedPeers.end());
    for (int i = 0; i < peers.size(); ++i) {
        if (seenPeers.find(peers.at(i)) == seenPeers.end()) {
            orderedPeers.push_back(peers.at(i));
            seenPeers.insert(peers.at(i));
        }
    }
    for (int i = 0; i < orderedPeers.size(); ++i) {
        if (!running) {
            break;
        }
        if (peerStatuses.find(orderedPeers.at(i)) != peerStatuses.end() && shouldSkipPeerForScore(peerStatuses[orderedPeers.at(i)])) {
            continue;
        }
        std::string response = httpPost(orderedPeers.at(i) + "/api/blocks/submit", blockJson);
        if (response.find("\"status\":\"accepted\"") != std::string::npos || response == "accepted") {
            continue;
        }

        std::string encodedBlock = urlEncode(blockJson);
        if (!encodedBlock.empty()) {
            httpGet(orderedPeers.at(i) + "/api/blocks/submit?block=" + encodedBlock);
        }
    }
}

void CLocalPeerClient::broadcastHandoff(const CFunctions::block_structure& block, const std::string& creatorPrivateKey)
{
    std::string body = handoffJson(block, creatorPrivateKey);
    if (body.empty()) {
        return;
    }

    std::vector<std::string> localPeers = getPriorityPeersForUpcomingCreators(20);
    std::set<std::string> seenPeers(localPeers.begin(), localPeers.end());
    std::vector<std::string> allPeers = getPeers();
    for (int i = 0; i < allPeers.size(); ++i) {
        if (seenPeers.find(allPeers.at(i)) == seenPeers.end()) {
            localPeers.push_back(allPeers.at(i));
            seenPeers.insert(allPeers.at(i));
        }
    }
    for (int i = 0; i < localPeers.size(); ++i) {
        std::string peer = normalizePeerUrl(localPeers.at(i));
        if (peer.empty()) {
            continue;
        }
        httpPost(peer + "/api/handoff/submit", body);
    }
}
