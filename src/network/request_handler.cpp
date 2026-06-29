//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "request_handler.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <curl/curl.h>
#include "mime_types.h"
#include "reply.h"
#include "request.h"
#include "ecdsacrypto.h"
#include "wallet.h"
#include "blockdb.h"
#include "functions/functions.h"
#include "network/localpeerclient.h"
#include "network/relayclient.h"
#include "networkconfig.h"

#include "networktime.h"

namespace http {
namespace server3 {

namespace {

const double API_DEFAULT_TRANSACTION_FEE = 0.0;

size_t request_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  ((std::string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

std::string request_http_get(const std::string& url)
{
  std::string read_buffer;
  curl_global_init(CURL_GLOBAL_ALL);
  CURL *curl = curl_easy_init();
  if (!curl)
  {
    return read_buffer;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  return read_buffer;
}
const double API_MAX_TRANSACTION_FEE = 0.1;
const char* API_SETTINGS_FILE = "settings.dat";

std::string json_escape(const std::string& value)
{
  std::stringstream escaped;
  for (std::size_t i = 0; i < value.length(); ++i)
  {
    char c = value[i];
    if (c == '"' || c == '\\')
    {
      escaped << '\\' << c;
    }
    else if (c == '\n')
    {
      escaped << "\\n";
    }
    else if (c == '\r')
    {
      escaped << "\\r";
    }
    else
    {
      escaped << c;
    }
  }
  return escaped.str();
}

std::string query_value(const std::string& path, const std::string& key)
{
  std::string token = key + "=";
  std::size_t query_start = path.find("?");
  if (query_start == std::string::npos)
  {
    return "";
  }

  std::size_t value_start = path.find(token, query_start + 1);
  if (value_start == std::string::npos)
  {
    return "";
  }
  value_start += token.length();

  std::size_t value_end = path.find("&", value_start);
  if (value_end == std::string::npos)
  {
    value_end = path.length();
  }

  return path.substr(value_start, value_end - value_start);
}

bool form_url_decode(const std::string& in, std::string& out)
{
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 3 > in.size())
      {
        return false;
      }

      int value;
      std::istringstream is(in.substr(i + 1, 2));
      if (!(is >> std::hex >> value))
      {
        return false;
      }
      out += static_cast<char>(value);
      i += 2;
    }
    else if (in[i] == '+')
    {
      out += ' ';
    }
    else
    {
      out += in[i];
    }
  }
  return true;
}

std::string submitted_value(const request& req, const std::string& request_path, const std::string& key)
{
  if (req.method == "POST" && !req.body.empty())
  {
    std::string token = key + "=";
    std::size_t value_start = req.body.find(token);
    if (value_start != std::string::npos)
    {
      value_start += token.length();
      std::size_t value_end = req.body.find("&", value_start);
      if (value_end == std::string::npos)
      {
        value_end = req.body.length();
      }

      std::string encoded_value = req.body.substr(value_start, value_end - value_start);
      std::string decoded_value;
      if (form_url_decode(encoded_value, decoded_value))
      {
        return decoded_value;
      }
      return encoded_value;
    }

    return req.body;
  }

  return query_value(request_path, key);
}

void text_reply(reply& rep, reply::status_type status, const std::string& content, const std::string& content_type)
{
  rep.status = status;
  rep.content = content;
  rep.headers.resize(2);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = boost::lexical_cast<std::string>(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = content_type;
}

bool parse_amount_value(const std::string& value, double& amount)
{
  char* end = NULL;
  amount = std::strtod(value.c_str(), &end);
  return end != value.c_str() && *end == '\0';
}

bool parse_fee_amount(const std::string& value, double& fee)
{
  if (parse_amount_value(value, fee) == false)
  {
    return false;
  }
  return fee >= 0.0 && fee <= API_MAX_TRANSACTION_FEE;
}

double api_default_transaction_fee()
{
  std::ifstream infile(API_SETTINGS_FILE);
  std::string line;
  while (std::getline(infile, line))
  {
    std::size_t start = line.find("fee:");
    if (start == 0)
    {
      std::string value = line.substr(4);
      double fee = API_DEFAULT_TRANSACTION_FEE;
      if (parse_fee_amount(value, fee))
      {
        return fee;
      }
    }
  }
  return API_DEFAULT_TRANSACTION_FEE;
}

long connected_block_count(CBlockDB& block_db)
{
  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  if (first_block_id < 0 || latest_block_id < 0)
  {
    return 0;
  }

  CFunctions::block_structure block = block_db.getBlock(first_block_id);
  std::set<std::string> seen_hashes;
  long count = 0;

  while (block.number > 0 && count < 1000000)
  {
    if (seen_hashes.count(block.hash) > 0)
    {
      break;
    }
    seen_hashes.insert(block.hash);
    ++count;

    if (block.number == latest_block_id)
    {
      break;
    }

    CFunctions::block_structure next_block = block_db.getNextBlock(block);
    if (next_block.number <= 0)
    {
      break;
    }
    block = next_block;
  }

  return count;
}

bool queue_and_broadcast_record(CFunctions& functions, CFunctions::record_structure record, std::string& error)
{
  if (functions.isRecordSizeValid(record) == false)
  {
    error = functions.recordSizeError(record);
    return false;
  }

  if (functions.addToQueue(record) == 0)
  {
    error = "unable to queue record";
    return false;
  }

  CRelayClient relay_client;
  relay_client.sendRecord(record);
  CLocalPeerClient::broadcastRecord(record);
  return true;
}

std::string normalized_public_name(const std::string& name)
{
  if (name.length() < 3 || name.length() > 32)
  {
    return "";
  }

  std::string normalized;
  for (std::size_t i = 0; i < name.length(); ++i)
  {
    unsigned char ch = static_cast<unsigned char>(name.at(i));
    if (std::isalnum(ch))
    {
      normalized.push_back(static_cast<char>(std::tolower(ch)));
    }
    else if (ch == '-' || ch == '_')
    {
      normalized.push_back(static_cast<char>(ch));
    }
    else
    {
      return "";
    }
  }
  return normalized;
}

int member_index_by_key(const std::vector<CFunctions::record_structure>& members, const std::string& public_key)
{
  for (int i = 0; i < members.size(); ++i)
  {
    if (members.at(i).sender_public_key.compare(public_key) == 0)
    {
      return i;
    }
  }
  return -1;
}

bool claim_public_name(std::vector<CFunctions::record_structure>& members,
                       std::map<std::string, std::string>& name_owners,
                       const std::string& public_key,
                       const std::string& name)
{
  std::string normalized = normalized_public_name(name);
  if (normalized.length() == 0)
  {
    return false;
  }

  int member_index = member_index_by_key(members, public_key);
  if (member_index < 0)
  {
    return false;
  }

  std::map<std::string, std::string>::iterator owner = name_owners.find(normalized);
  if (owner != name_owners.end() && owner->second.compare(public_key) != 0)
  {
    return false;
  }

  std::string previous_name = normalized_public_name(members.at(member_index).name);
  if (previous_name.length() > 0)
  {
    std::map<std::string, std::string>::iterator previous_owner = name_owners.find(previous_name);
    if (previous_owner != name_owners.end() && previous_owner->second.compare(public_key) == 0)
    {
      name_owners.erase(previous_owner);
    }
  }

  members[member_index].name = name;
  name_owners[normalized] = public_key;
  return true;
}

std::vector<CFunctions::record_structure> accepted_membership_records(CBlockDB& block_db)
{
  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  std::vector<CFunctions::record_structure> members;
  if (first_block_id < 0 || latest_block_id < 0)
  {
    return members;
  }

  CFunctions::block_structure block = block_db.getBlock(first_block_id);
  std::map<std::string, std::string> name_owners;
  int guard = 0;
  while (block.number > 0 && guard < 100000)
  {
    for (int i = 0; i < block.records.size(); ++i)
    {
      CFunctions::record_structure record = block.records.at(i);
      if (record.transaction_type == CFunctions::JOIN_NETWORK)
      {
        bool exists = false;
        for (int m = 0; m < members.size(); ++m)
        {
          if (members.at(m).sender_public_key.compare(record.sender_public_key) == 0)
          {
            exists = true;
          }
        }
        if (exists == false)
        {
          std::string requested_name = record.name;
          record.name = "";
          members.push_back(record);
          if (requested_name.length() > 0)
          {
            claim_public_name(members, name_owners, record.sender_public_key, requested_name);
          }
        }
      }
      if (record.transaction_type == CFunctions::UPDATE_NAME &&
          record.sender_public_key.length() > 0 &&
          record.name.length() > 0)
      {
        claim_public_name(members, name_owners, record.sender_public_key, record.name);
      }
    }

    if (block.number == latest_block_id)
    {
      break;
    }
    CFunctions::block_structure next_block = block_db.getNextBlock(block);
    if (next_block.number <= 0 || next_block.number == block.number)
    {
      break;
    }
    block = next_block;
    ++guard;
  }
  return members;
}

std::map<std::string, std::string> accepted_member_names(CBlockDB& block_db)
{
  std::map<std::string, std::string> names;
  std::vector<CFunctions::record_structure> members = accepted_membership_records(block_db);
  for (int i = 0; i < members.size(); ++i)
  {
    CFunctions::record_structure member = members.at(i);
    if (member.sender_public_key.length() > 0 && member.name.length() > 0)
    {
      names[member.sender_public_key] = member.name;
    }
  }
  return names;
}

std::vector<CFunctions::record_structure> active_membership_records(CBlockDB& block_db)
{
  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  std::vector<CFunctions::record_structure> active_members;
  if (first_block_id < 0 || latest_block_id < 0)
  {
    return active_members;
  }

  std::vector<CFunctions::record_structure> members;
  std::map<std::string, long> latest_heartbeat_block_by_user;
  std::map<std::string, std::string> name_owners;
  bool saw_heartbeat = false;
  CFunctions::block_structure block = block_db.getBlock(first_block_id);
  int guard = 0;
  while (block.number > 0 && guard < 100000)
  {
    for (int i = 0; i < block.records.size(); ++i)
    {
      CFunctions::record_structure record = block.records.at(i);
      if (record.transaction_type == CFunctions::JOIN_NETWORK)
      {
        bool exists = false;
        for (int m = 0; m < members.size(); ++m)
        {
          if (members.at(m).sender_public_key.compare(record.sender_public_key) == 0)
          {
            exists = true;
          }
        }
        if (exists == false)
        {
          std::string requested_name = record.name;
          record.name = "";
          members.push_back(record);
          if (requested_name.length() > 0)
          {
            claim_public_name(members, name_owners, record.sender_public_key, requested_name);
          }
        }
      }
      if (record.transaction_type == CFunctions::UPDATE_NAME &&
          record.sender_public_key.length() > 0 &&
          record.name.length() > 0)
      {
        claim_public_name(members, name_owners, record.sender_public_key, record.name);
      }
      if (record.transaction_type == CFunctions::HEART_BEAT &&
          record.sender_public_key.length() > 0)
      {
        saw_heartbeat = true;
        latest_heartbeat_block_by_user[record.sender_public_key] = block.number;
      }
    }

    if (block.number == latest_block_id)
    {
      break;
    }
    CFunctions::block_structure next_block = block_db.getNextBlock(block);
    if (next_block.number <= 0 || next_block.number == block.number)
    {
      break;
    }
    block = next_block;
    ++guard;
  }

  CNetworkTime net_time;
  long heartbeat_cutoff = (net_time.getEpoch() / 15) - CFunctions::HEARTBEAT_VALID_BLOCKS;
  for (int i = 0; i < members.size(); ++i)
  {
    std::string member_key = members.at(i).sender_public_key;
    if (saw_heartbeat == false ||
        (latest_heartbeat_block_by_user.find(member_key) != latest_heartbeat_block_by_user.end() &&
         latest_heartbeat_block_by_user[member_key] >= heartbeat_cutoff))
    {
      active_members.push_back(members.at(i));
    }
  }

  return active_members;
}

bool public_name_available_for_owner(CBlockDB& block_db, const std::string& name, const std::string& owner_public_key)
{
  std::string normalized = normalized_public_name(name);
  if (normalized.length() == 0)
  {
    return false;
  }

  std::vector<CFunctions::record_structure> members = accepted_membership_records(block_db);
  for (int i = 0; i < members.size(); ++i)
  {
    std::string member_name = normalized_public_name(members.at(i).name);
    if (member_name.compare(normalized) == 0 &&
        members.at(i).sender_public_key.compare(owner_public_key) != 0)
    {
      return false;
    }
  }
  return true;
}

struct wallet_history_record
{
  std::string direction;
  long block_number;
  int record_index;
  CFunctions::record_structure record;
  std::string from_key;
  std::string to_key;
  double net_amount;
};

void add_wallet_history_record(std::deque<wallet_history_record>& history,
                               int limit,
                               const std::string& direction,
                               long block_number,
                               int record_index,
                               const CFunctions::record_structure& record,
                               const std::string& from_key,
                               const std::string& to_key,
                               double net_amount)
{
  wallet_history_record item;
  item.direction = direction;
  item.block_number = block_number;
  item.record_index = record_index;
  item.record = record;
  item.from_key = from_key;
  item.to_key = to_key;
  item.net_amount = net_amount;
  history.push_back(item);
  if (history.size() > limit)
  {
    history.pop_front();
  }
}

std::string name_for_key(const std::map<std::string, std::string>& names, const std::string& public_key)
{
  std::map<std::string, std::string>::const_iterator it = names.find(public_key);
  if (it != names.end())
  {
    return it->second;
  }
  return "";
}

std::string record_type_name(CFunctions::transaction_types type)
{
  switch (type)
  {
    case CFunctions::JOIN_NETWORK:
      return "JOIN_NETWORK";
    case CFunctions::ISSUE_CURRENCY:
      return "ISSUE_CURRENCY";
    case CFunctions::TRANSFER_CURRENCY:
      return "TRANSFER_CURRENCY";
    case CFunctions::CARRY_FORWARD:
      return "CARRY_FORWARD";
    case CFunctions::PERIOD_SUMMARY:
      return "PERIOD_SUMMARY";
    case CFunctions::VOTE:
      return "VOTE";
    case CFunctions::HEART_BEAT:
      return "HEART_BEAT";
    case CFunctions::UPDATE_NAME:
      return "UPDATE_NAME";
  }
  return "UNKNOWN";
}

std::string mempool_json()
{
  CFunctions functions;
  std::vector<CFunctions::record_structure> records = functions.peekQueueRecords();
  std::stringstream ss;
  ss << "{\"status\":\"ok\",\"records\":[";
  for (int i = 0; i < records.size(); ++i)
  {
    CFunctions::record_structure record = records.at(i);
    if (i > 0)
    {
      ss << ",";
    }
    ss << "{";
    ss << "\"index\":\"" << i << "\",";
    ss << "\"type\":\"" << record_type_name(record.transaction_type) << "\",";
    ss << "\"network\":\"" << json_escape(record.network) << "\",";
    ss << "\"time\":\"" << json_escape(record.time) << "\",";
    ss << "\"amount\":\"" << record.amount << "\",";
    ss << "\"fee\":\"" << record.fee << "\",";
    ss << "\"from_key\":\"" << json_escape(record.sender_public_key) << "\",";
    ss << "\"to_key\":\"" << json_escape(record.recipient_public_key) << "\",";
    ss << "\"member_key\":\"" << json_escape(record.sender_public_key) << "\",";
    ss << "\"name\":\"" << json_escape(record.name) << "\",";
    ss << "\"value\":\"" << json_escape(record.value) << "\",";
    ss << "\"hash\":\"" << json_escape(record.hash) << "\"";
    ss << "}";
  }
  ss << "]}";
  return ss.str();
}

bool best_peer_status(const std::vector<CLocalPeerClient::peer_status>& local_peers,
                      CLocalPeerClient::peer_status& best_peer)
{
  bool found = false;
  for (int i = 0; i < local_peers.size(); ++i)
  {
    CLocalPeerClient::peer_status peer = local_peers.at(i);
    if (peer.reachable == false || peer.genesisMatch == false)
    {
      continue;
    }
    if (found == false || peer.latestBlockId > best_peer.latestBlockId)
    {
      best_peer = peer;
      found = true;
    }
  }
  return found;
}

std::map<long, std::string> peer_hashes_for_blocks(const std::deque<CFunctions::block_structure>& blocks,
                                                   const CLocalPeerClient::peer_status& peer)
{
  std::map<long, std::string> hashes;
  if (peer.url.length() == 0 || peer.reachable == false || peer.genesisMatch == false)
  {
    return hashes;
  }

  CFunctions functions;
  for (std::deque<CFunctions::block_structure>::const_iterator it = blocks.begin();
       it != blocks.end();
       ++it)
  {
    if (it->number <= 0 || it->number > peer.latestBlockId)
    {
      continue;
    }

    std::stringstream url;
    url << peer.url << "/api/blocks/" << it->number;
    std::string response = request_http_get(url.str());
    if (response.empty())
    {
      continue;
    }

    std::vector<CFunctions::block_structure> peer_blocks = functions.parseBlockJson(response);
    if (peer_blocks.size() > 0 && peer_blocks.at(0).number == it->number)
    {
      hashes[it->number] = peer_blocks.at(0).hash;
    }
  }
  return hashes;
}

std::string recent_blockchain_json(CBlockDB& block_db)
{
  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  if (first_block_id < 0 || latest_block_id < 0)
  {
    return "{\"status\":\"ok\",\"limit\":\"50\",\"blocks\":[]}";
  }

  const int limit = 50;
  std::deque<CFunctions::block_structure> recent_blocks;
  CFunctions::block_structure block = block_db.getBlock(first_block_id);
  int guard = 0;
  while (block.number > 0 && guard < 100000)
  {
    recent_blocks.push_back(block);
    if (recent_blocks.size() > limit)
    {
      recent_blocks.pop_front();
    }

    if (block.number == latest_block_id)
    {
      break;
    }

    CFunctions::block_structure next_block = block_db.getNextBlock(block);
    if (next_block.number <= 0 || next_block.number == block.number)
    {
      break;
    }
    block = next_block;
    ++guard;
  }

  std::vector<CLocalPeerClient::peer_status> peers = CLocalPeerClient::getPeerStatuses();
  CLocalPeerClient::peer_status best_peer;
  bool has_best_peer = best_peer_status(peers, best_peer);
  std::map<long, std::string> peer_hashes;
  if (has_best_peer)
  {
    peer_hashes = peer_hashes_for_blocks(recent_blocks, best_peer);
  }

  std::map<std::string, std::string> names = accepted_member_names(block_db);
  std::stringstream ss;
  ss << "{\"status\":\"ok\",";
  ss << "\"limit\":\"" << limit << "\",";
  ss << "\"blocks\":[";
  for (std::deque<CFunctions::block_structure>::reverse_iterator it = recent_blocks.rbegin();
       it != recent_blocks.rend();
       ++it)
  {
    if (it != recent_blocks.rbegin())
    {
      ss << ",";
    }
    CFunctions::block_structure current_block = *it;
    std::string network_status = "unknown";
    std::string peer_hash = "";
    if (has_best_peer)
    {
      if (current_block.number > best_peer.latestBlockId)
      {
        network_status = "ahead";
      }
      else
      {
        std::map<long, std::string>::iterator peer_hash_it = peer_hashes.find(current_block.number);
        if (peer_hash_it != peer_hashes.end())
        {
          peer_hash = peer_hash_it->second;
          network_status = current_block.hash.compare(peer_hash) == 0 ? "match" : "mismatch";
        }
      }
    }
    ss << "{";
    ss << "\"number\":\"" << current_block.number << "\",";
    ss << "\"network_status\":\"" << network_status << "\",";
    ss << "\"peer_hash\":\"" << json_escape(peer_hash) << "\",";
    ss << "\"peer_url\":\"" << json_escape(has_best_peer ? best_peer.url : "") << "\",";
    ss << "\"time\":\"" << json_escape(current_block.time) << "\",";
    ss << "\"previous_block_id\":\"" << current_block.previous_block_id << "\",";
    ss << "\"creator_key\":\"" << json_escape(current_block.creator_key) << "\",";
    ss << "\"creator_name\":\"" << json_escape(name_for_key(names, current_block.creator_key)) << "\",";
    ss << "\"hash\":\"" << json_escape(current_block.hash) << "\",";
    ss << "\"previous_hash\":\"" << json_escape(current_block.previous_block_hash) << "\",";
    ss << "\"record_count\":\"" << current_block.records.size() << "\",";
    ss << "\"records\":[";
    for (int r = 0; r < current_block.records.size(); ++r)
    {
      if (r > 0)
      {
        ss << ",";
      }
      CFunctions::record_structure record = current_block.records.at(r);
      ss << "{";
      ss << "\"index\":\"" << r << "\",";
      ss << "\"type\":\"" << record_type_name(record.transaction_type) << "\",";
      ss << "\"network\":\"" << json_escape(record.network) << "\",";
      ss << "\"time\":\"" << json_escape(record.time) << "\",";
      ss << "\"amount\":\"" << record.amount << "\",";
      ss << "\"fee\":\"" << record.fee << "\",";
      ss << "\"from_key\":\"" << json_escape(record.sender_public_key) << "\",";
      ss << "\"from_name\":\"" << json_escape(name_for_key(names, record.sender_public_key)) << "\",";
      ss << "\"to_key\":\"" << json_escape(record.recipient_public_key) << "\",";
      ss << "\"to_name\":\"" << json_escape(name_for_key(names, record.recipient_public_key)) << "\",";
      ss << "\"member_key\":\"" << json_escape(record.sender_public_key) << "\",";
      ss << "\"member_name\":\"" << json_escape(name_for_key(names, record.sender_public_key)) << "\",";
      ss << "\"name\":\"" << json_escape(record.name) << "\",";
      ss << "\"value\":\"" << json_escape(record.value) << "\",";
      ss << "\"hash\":\"" << json_escape(record.hash) << "\"";
      ss << "}";
    }
    ss << "]";
    ss << "}";
  }
  ss << "]}";
  return ss.str();
}

void add_balance_delta(std::map<std::string, double>& balances, const std::string& public_key, double amount)
{
  if (public_key.length() == 0)
  {
    return;
  }
  balances[public_key] += amount;
}

long parse_carry_forward_value_long(const std::string& value, const std::string& key)
{
  std::string prefix = key + "=";
  std::size_t start = value.find(prefix);
  if (start == std::string::npos)
  {
    return -1;
  }
  start += prefix.length();
  std::size_t end = value.find(";", start);
  std::string section = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
  if (section.length() == 0)
  {
    return -1;
  }
  return ::atol(section.c_str());
}

std::string carry_forward_unique_key(const CFunctions::record_structure& record)
{
  long period = parse_carry_forward_value_long(record.value, "period");
  if (record.recipient_public_key.length() == 0 || period < 0)
  {
    return "";
  }

  std::stringstream ss;
  ss << record.recipient_public_key << ":" << period;
  return ss.str();
}

std::map<std::string, double> accepted_ledger_balances(CBlockDB& block_db)
{
  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  std::map<std::string, double> balances;
  if (first_block_id < 0 || latest_block_id < 0)
  {
    return balances;
  }

  CFunctions::block_structure block = block_db.getBlock(first_block_id);
  std::set<std::string> accepted_record_hashes;
  std::set<std::string> accepted_carry_forward_keys;
  int guard = 0;
  while (block.number > 0 && guard < 100000)
  {
    for (int i = 0; i < block.records.size(); ++i)
    {
      CFunctions::record_structure record = block.records.at(i);
      if (record.hash.length() > 0 && accepted_record_hashes.find(record.hash) != accepted_record_hashes.end())
      {
        continue;
      }
      if (record.hash.length() > 0)
      {
        accepted_record_hashes.insert(record.hash);
      }

      if (record.transaction_type == CFunctions::ISSUE_CURRENCY)
      {
        add_balance_delta(balances, record.recipient_public_key, record.amount);
      }
      else if (record.transaction_type == CFunctions::TRANSFER_CURRENCY)
      {
        add_balance_delta(balances, record.recipient_public_key, record.amount);
        add_balance_delta(balances, record.sender_public_key, -record.amount - record.fee);
        add_balance_delta(balances, block.creator_key, record.fee);
      }
      else if (record.transaction_type == CFunctions::VOTE)
      {
        add_balance_delta(balances, record.sender_public_key, -record.fee);
        add_balance_delta(balances, block.creator_key, record.fee);
      }
      else if (record.transaction_type == CFunctions::CARRY_FORWARD)
      {
        std::string carry_forward_key = carry_forward_unique_key(record);
        if (carry_forward_key.length() > 0 &&
            accepted_carry_forward_keys.find(carry_forward_key) == accepted_carry_forward_keys.end())
        {
          accepted_carry_forward_keys.insert(carry_forward_key);
          add_balance_delta(balances, record.sender_public_key, CFunctions::CARRY_FORWARD_REWARD);
        }
      }
    }

    if (block.number == latest_block_id)
    {
      break;
    }
    CFunctions::block_structure next_block = block_db.getNextBlock(block);
    if (next_block.number <= 0 || next_block.number == block.number)
    {
      break;
    }
    block = next_block;
    ++guard;
  }
  return balances;
}

double accepted_member_supply(CBlockDB& block_db)
{
  std::vector<CFunctions::record_structure> members = accepted_membership_records(block_db);
  std::map<std::string, double> balances = accepted_ledger_balances(block_db);
  double supply = 0.0;
  for (int i = 0; i < members.size(); ++i)
  {
    std::string public_key = members.at(i).sender_public_key;
    if (public_key.length() > 0)
    {
      supply += balances[public_key];
    }
  }
  return supply;
}

double ledger_balance_total(const std::map<std::string, double>& balances)
{
  double total = 0.0;
  for (std::map<std::string, double>::const_iterator it = balances.begin(); it != balances.end(); ++it)
  {
    total += it->second;
  }
  return total;
}

std::string network_users_json(CBlockDB& block_db)
{
  std::vector<CFunctions::record_structure> members = accepted_membership_records(block_db);
  std::map<std::string, double> balances = accepted_ledger_balances(block_db);
  std::stringstream ss;
  ss << "{\"status\":\"ok\",\"users\":[";
  bool wrote_user = false;
  for (int i = 0; i < members.size(); ++i)
  {
    CFunctions::record_structure member = members.at(i);
    if (member.sender_public_key.length() == 0)
    {
      continue;
    }
    if (wrote_user)
    {
      ss << ",";
    }
    wrote_user = true;
    ss << "{";
    ss << "\"public_key\":\"" << json_escape(member.sender_public_key) << "\",";
    ss << "\"name\":\"" << json_escape(member.name) << "\",";
    ss << "\"balance\":\"" << balances[member.sender_public_key] << "\"";
    ss << "}";
  }
  ss << "]}";
  return ss.str();
}

std::string wallet_history_json(CBlockDB& block_db, const std::string& public_key)
{
  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  if (first_block_id < 0 || latest_block_id < 0)
  {
    return "{\"status\":\"ok\",\"records\":[]}";
  }

  const int limit = 500;
  std::deque<wallet_history_record> history;
  std::set<std::string> accepted_record_hashes;
  CFunctions::block_structure block = block_db.getBlock(first_block_id);
  int guard = 0;
  while (block.number > 0 && guard < 100000)
  {
    for (int i = 0; i < block.records.size(); ++i)
    {
      CFunctions::record_structure record = block.records.at(i);
      if (record.hash.length() > 0 && accepted_record_hashes.find(record.hash) != accepted_record_hashes.end())
      {
        continue;
      }
      if (record.hash.length() > 0)
      {
        accepted_record_hashes.insert(record.hash);
      }

      if (record.transaction_type == CFunctions::ISSUE_CURRENCY &&
          record.recipient_public_key.compare(public_key) == 0)
      {
        add_wallet_history_record(history, limit, "REWARD", block.number, i, record,
                                  record.sender_public_key, record.recipient_public_key, record.amount);
      }

      if (record.transaction_type == CFunctions::TRANSFER_CURRENCY)
      {
        bool sent_by_wallet = record.sender_public_key.compare(public_key) == 0;
        bool received_by_wallet = record.recipient_public_key.compare(public_key) == 0;

        if (sent_by_wallet && received_by_wallet)
        {
          add_wallet_history_record(history, limit, "SELF", block.number, i, record,
                                    public_key, public_key, 0);
        }
        else if (sent_by_wallet)
        {
          add_wallet_history_record(history, limit, "SENT", block.number, i, record,
                                    record.sender_public_key, record.recipient_public_key,
                                    -(record.amount + record.fee));
        }
        else if (received_by_wallet)
        {
          add_wallet_history_record(history, limit, "RECEIVED", block.number, i, record,
                                    record.sender_public_key, record.recipient_public_key,
                                    record.amount);
        }
      }

      if ((record.transaction_type == CFunctions::TRANSFER_CURRENCY ||
           record.transaction_type == CFunctions::VOTE) &&
          block.creator_key.compare(public_key) == 0 &&
          record.fee > 0)
      {
        add_wallet_history_record(history, limit, "FEE", block.number, i, record,
                                  record.sender_public_key, block.creator_key, record.fee);
      }

      if (record.transaction_type == CFunctions::CARRY_FORWARD &&
          record.sender_public_key.compare(public_key) == 0)
      {
        add_wallet_history_record(history, limit, "CARRY_FORWARD", block.number, i, record,
                                  record.sender_public_key, record.recipient_public_key,
                                  record.amount + CFunctions::CARRY_FORWARD_REWARD);
      }
    }

    if (block.number == latest_block_id)
    {
      break;
    }
    CFunctions::block_structure next_block = block_db.getNextBlock(block);
    if (next_block.number <= 0 || next_block.number == block.number)
    {
      break;
    }
    block = next_block;
    ++guard;
  }

  std::map<std::string, std::string> names = accepted_member_names(block_db);
  std::stringstream ss;
  ss << "{\"status\":\"ok\",\"records\":[";
  for (int i = 0; i < history.size(); ++i)
  {
    wallet_history_record item = history.at(i);
    if (i > 0)
    {
      ss << ",";
    }
    ss << "{";
    ss << "\"direction\":\"" << item.direction << "\",";
    ss << "\"block\":\"" << item.block_number << "\",";
    ss << "\"index\":\"" << item.record_index << "\",";
    ss << "\"time\":\"" << json_escape(item.record.time) << "\",";
    ss << "\"net\":\"" << item.net_amount << "\",";
    ss << "\"amount\":\"" << item.record.amount << "\",";
    ss << "\"fee\":\"" << item.record.fee << "\",";
    ss << "\"from_key\":\"" << json_escape(item.from_key) << "\",";
    ss << "\"from_name\":\"" << json_escape(name_for_key(names, item.from_key)) << "\",";
    ss << "\"to_key\":\"" << json_escape(item.to_key) << "\",";
    ss << "\"to_name\":\"" << json_escape(name_for_key(names, item.to_key)) << "\",";
    ss << "\"hash\":\"" << json_escape(item.record.hash) << "\"";
    ss << "}";
  }
  ss << "]}";
  return ss.str();
}

double sync_progress_percent(long first_block_id,
                             long latest_block_id,
                             const std::vector<CLocalPeerClient::peer_status>& local_peers)
{
  if (latest_block_id < 0)
  {
    return 0.0;
  }

  long best_peer_latest_block_id = latest_block_id;
  for (int i = 0; i < local_peers.size(); ++i)
  {
    CLocalPeerClient::peer_status peer = local_peers.at(i);
    if (peer.reachable &&
        peer.genesisMatch &&
        peer.latestBlockId > best_peer_latest_block_id)
    {
      best_peer_latest_block_id = peer.latestBlockId;
    }
  }

  if (first_block_id < 0 || best_peer_latest_block_id <= latest_block_id)
  {
    return 100.0;
  }

  long total_span = best_peer_latest_block_id - first_block_id;
  long local_span = latest_block_id - first_block_id;
  if (total_span <= 0)
  {
    return 100.0;
  }

  double progress = (static_cast<double>(local_span) / static_cast<double>(total_span)) * 100.0;
  if (progress < 0.0)
  {
    return 0.0;
  }
  if (progress > 100.0)
  {
    return 100.0;
  }
  return progress;
}

long best_peer_latest_block_id(const std::vector<CLocalPeerClient::peer_status>& local_peers)
{
  long best_peer_latest_block_id = -1;
  for (int i = 0; i < local_peers.size(); ++i)
  {
    CLocalPeerClient::peer_status peer = local_peers.at(i);
    if (peer.reachable &&
        peer.genesisMatch &&
        peer.latestBlockId > best_peer_latest_block_id)
    {
      best_peer_latest_block_id = peer.latestBlockId;
    }
  }
  return best_peer_latest_block_id;
}

bool local_chain_matches_peer(CBlockDB& block_db,
                              const CLocalPeerClient::peer_status& peer)
{
  if (peer.latestBlockId < 0 || peer.latestBlockHash.length() == 0)
  {
    return true;
  }

  CFunctions::block_structure local_peer_tip = block_db.getBlock(peer.latestBlockId);
  return local_peer_tip.hash.length() > 0 &&
         local_peer_tip.hash.compare(peer.latestBlockHash) == 0;
}

bool synced_with_peer_latest(CBlockDB& block_db,
                             long latest_block_id,
                             const std::vector<CLocalPeerClient::peer_status>& local_peers)
{
  CLocalPeerClient::peer_status peer;
  if (best_peer_status(local_peers, peer) == false)
  {
    return true;
  }
  return latest_block_id >= peer.latestBlockId &&
         local_chain_matches_peer(block_db, peer);
}

}

request_handler::request_handler(const std::string& doc_root)
  : doc_root_(doc_root)
{
}

void request_handler::handle_request(const request& req, reply& rep)
{

  // Decode url to path.
  std::string request_path;
  if (!url_decode(req.uri, request_path))
  {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  CBlockDB blockDB;
  CFunctions functions;

  if (request_path == "/api/status")
  {
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    CFunctions::block_structure firstBlock = blockDB.getBlock(firstBlockId);
    CFunctions::block_structure latestBlock = blockDB.getBlock(latestBlockId);
    CNetworkConfig config = CNetworkConfig::load();
    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"protocol_version\":\"" << CLocalPeerClient::PROTOCOL_VERSION << "\",";
    ss << "\"network\":\"" << config.network << "\",";
    ss << "\"first_block_id\":\"" << firstBlockId << "\",";
    ss << "\"first_block_hash\":\"" << firstBlock.hash << "\",";
    ss << "\"expected_genesis_block\":\"" << config.genesisBlock << "\",";
    ss << "\"expected_genesis_hash\":\"" << config.genesisHash << "\",";
    ss << "\"genesis_match\":\"" << (config.genesisMatches(firstBlockId, firstBlock.hash) ? "yes" : "no") << "\",";
    ss << "\"latest_block_id\":\"" << latestBlockId << "\",";
    ss << "\"latest_block_hash\":\"" << latestBlock.hash << "\"}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/peers")
  {
    std::vector<CLocalPeerClient::peer_status> peers = CLocalPeerClient::getPeerStatuses();
    std::stringstream ss;
    ss << "{\"protocol_version\":\"" << CLocalPeerClient::PROTOCOL_VERSION << "\",";
    ss << "\"peers\":[";
    for (int i = 0; i < peers.size(); ++i)
    {
      if (i > 0)
      {
        ss << ",";
      }
      ss << "{";
      ss << "\"url\":\"" << json_escape(peers.at(i).url) << "\",";
      ss << "\"latest_block_id\":\"" << peers.at(i).latestBlockId << "\",";
      ss << "\"latest_block_hash\":\"" << peers.at(i).latestBlockHash << "\",";
      ss << "\"genesis_match\":\"" << (peers.at(i).genesisMatch ? "yes" : "no") << "\",";
      ss << "\"reachable\":\"" << (peers.at(i).reachable ? "yes" : "no") << "\",";
      ss << "\"last_success_epoch\":\"" << peers.at(i).lastSuccessEpoch << "\",";
      ss << "\"first_failure_epoch\":\"" << peers.at(i).firstFailureEpoch << "\",";
      ss << "\"score\":\"" << peers.at(i).score << "\"";
      ss << "}";
    }
    ss << "]}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/wallet/status")
  {
    CWallet wallet;
    if (wallet.fileExists("wallet.dat") == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\"}", "application/json");
      return;
    }

    std::string privateKey;
    std::string publicKey;
    wallet.read(privateKey, publicKey);
    functions.scanChain(publicKey, false);

    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    long blockCount = connected_block_count(blockDB);
    CFunctions::block_structure latestBlock = latestBlockId > 0 ? blockDB.getBlock(latestBlockId) : CFunctions::block_structure();
    std::string latestBlockTime = latestBlock.time;
    if (latestBlockTime.length() == 0 && latestBlockId > 0)
    {
      std::stringstream latestBlockTimeStream;
      latestBlockTimeStream << (latestBlockId * 15);
      latestBlockTime = latestBlockTimeStream.str();
    }
    CNetworkConfig config = CNetworkConfig::load();
    CFunctions::block_structure firstBlock = blockDB.getBlock(firstBlockId);
    CNetworkTime netTime;
    std::vector<CLocalPeerClient::peer_status> localPeers = CLocalPeerClient::getPeerStatuses();
    long peerLatestBlockId = best_peer_latest_block_id(localPeers);
    CLocalPeerClient::peer_status bestPeer;
    bool hasBestPeer = best_peer_status(localPeers, bestPeer);
    bool peerChainMatch = hasBestPeer ? local_chain_matches_peer(blockDB, bestPeer) : true;
    bool peerSync = synced_with_peer_latest(blockDB, latestBlockId, localPeers);
    std::map<std::string, std::string> memberNames = accepted_member_names(blockDB);
    std::vector<CFunctions::record_structure> acceptedMembers = accepted_membership_records(blockDB);
    std::map<std::string, double> ledgerBalances = accepted_ledger_balances(blockDB);
    double ledgerBalanceTotal = ledger_balance_total(ledgerBalances);
    double supplyDifference = ledgerBalanceTotal - functions.currency_circulation;
    if (std::fabs(supplyDifference) < 0.000001)
    {
      supplyDifference = 0.0;
    }
    std::vector<CFunctions::record_structure> activeMembers = active_membership_records(blockDB);
    long currentTimeBlock = netTime.getEpoch() / 15;
    long nextTimeBlock = currentTimeBlock + 1;
    std::string currentCreator = "";
    std::string nextCreator = "";
    if (activeMembers.size() > 0)
    {
      currentCreator = activeMembers.at(currentTimeBlock % activeMembers.size()).sender_public_key;
      nextCreator = activeMembers.at(nextTimeBlock % activeMembers.size()).sender_public_key;
    }
    long secondsUntilNextBlock = (nextTimeBlock * 15) - netTime.getEpoch();
    if (secondsUntilNextBlock < 0)
    {
      secondsUntilNextBlock = 0;
    }

    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"public_key\":\"" << publicKey << "\",";
    ss << "\"public_name\":\"" << json_escape(name_for_key(memberNames, publicKey)) << "\",";
    ss << "\"active_member_count\":\"" << activeMembers.size() << "\",";
    ss << "\"current_block_id\":\"" << currentTimeBlock << "\",";
    ss << "\"current_block_creator\":\"" << json_escape(currentCreator) << "\",";
    ss << "\"current_block_creator_name\":\"" << json_escape(name_for_key(memberNames, currentCreator)) << "\",";
    ss << "\"current_block_creator_is_wallet\":\"" << (currentCreator.compare(publicKey) == 0 ? "yes" : "no") << "\",";
    ss << "\"next_block_id\":\"" << nextTimeBlock << "\",";
    ss << "\"next_block_creator\":\"" << json_escape(nextCreator) << "\",";
    ss << "\"next_block_creator_name\":\"" << json_escape(name_for_key(memberNames, nextCreator)) << "\",";
    ss << "\"next_block_creator_is_wallet\":\"" << (nextCreator.compare(publicKey) == 0 ? "yes" : "no") << "\",";
    ss << "\"seconds_until_next_block\":\"" << secondsUntilNextBlock << "\",";
    ss << "\"balance\":\"" << functions.balance << "\",";
    ss << "\"transaction_fee\":\"" << api_default_transaction_fee() << "\",";
    ss << "\"joined\":\"" << (functions.joined ? "yes" : "no") << "\",";
    ss << "\"active_heartbeat\":\"" << (functions.active_heartbeat ? "yes" : "no") << "\",";
    ss << "\"heartbeat_renewal_due\":\"" << (functions.heartbeat_renewal_due ? "yes" : "no") << "\",";
    ss << "\"last_heartbeat_block\":\"" << functions.last_heartbeat_block << "\",";
    ss << "\"currency_supply\":\"" << functions.currency_circulation << "\",";
    ss << "\"ledger_balance_total\":\"" << ledgerBalanceTotal << "\",";
    ss << "\"supply_difference\":\"" << supplyDifference << "\",";
    ss << "\"user_count\":\"" << acceptedMembers.size() << "\",";
    ss << "\"network_up_to_date\":\"" << (functions.IsChainUpToDate() ? "yes" : "no") << "\",";
    ss << "\"peer_sync\":\"" << (peerSync ? "yes" : "no") << "\",";
    ss << "\"peer_latest_block_id\":\"" << peerLatestBlockId << "\",";
    ss << "\"peer_latest_block_hash\":\"" << json_escape(hasBestPeer ? bestPeer.latestBlockHash : "") << "\",";
    ss << "\"peer_chain_match\":\"" << (peerChainMatch ? "yes" : "no") << "\",";
    ss << "\"sync_progress\":\"" << sync_progress_percent(firstBlockId, latestBlockId, localPeers) << "\",";
    ss << "\"first_block_id\":\"" << firstBlockId << "\",";
    ss << "\"latest_block_id\":\"" << latestBlockId << "\",";
    ss << "\"latest_block_hash\":\"" << json_escape(latestBlock.hash) << "\",";
    ss << "\"latest_block_time\":\"" << json_escape(latestBlockTime) << "\",";
    ss << "\"block_count\":\"" << blockCount << "\",";
    ss << "\"genesis_match\":\"" << (config.genesisMatches(firstBlockId, firstBlock.hash) ? "yes" : "no") << "\",";
    ss << "\"network_time_offset\":\"" << netTime.getOffset() << "\",";
    ss << "\"local_peers\":\"" << localPeers.size() << "\"}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/wallet/history")
  {
    CWallet wallet;
    if (wallet.fileExists("wallet.dat") == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"records\":[]}", "application/json");
      return;
    }

    std::string privateKey;
    std::string publicKey;
    wallet.read(privateKey, publicKey);
    text_reply(rep, reply::ok, wallet_history_json(blockDB, publicKey), "application/json");
    return;
  }

  if (request_path.find("/api/wallet/setname?") == 0
      || (req.method == "POST" && request_path == "/api/wallet/setname"))
  {
    CWallet wallet;
    if (wallet.fileExists("wallet.dat") == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"message\":\"Wallet file not found.\"}", "application/json");
      return;
    }

    std::string name = submitted_value(req, request_path, "name");
    if (normalized_public_name(name).length() == 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Use 3-32 characters: letters, numbers, dash, or underscore.\"}", "application/json");
      return;
    }

    std::string privateKey;
    std::string publicKey;
    wallet.read(privateKey, publicKey);
    functions.scanChain(publicKey, false);
    if (functions.joined == false)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Join the network before updating your public name.\"}", "application/json");
      return;
    }
    if (public_name_available_for_owner(blockDB, name, publicKey) == false)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"That public name is already taken.\"}", "application/json");
      return;
    }

    CFunctions::record_structure nameRecord;
    nameRecord.network = "main";
    CNetworkTime netTime;
    std::stringstream time_stream;
    time_stream << netTime.getEpoch();
    nameRecord.time = time_stream.str();
    nameRecord.transaction_type = CFunctions::UPDATE_NAME;
    nameRecord.amount = 0.0;
    nameRecord.fee = 0.0;
    nameRecord.sender_public_key = publicKey;
    nameRecord.recipient_public_key = "";
    nameRecord.name = name;
    nameRecord.hash = functions.getRecordHash(nameRecord);

    CECDSACrypto ecdsa;
    std::string signature = "";
    ecdsa.SignMessage(privateKey, nameRecord.hash, signature);
    nameRecord.signature = signature;

    std::string queue_error;
    if (queue_and_broadcast_record(functions, nameRecord, queue_error) == false)
    {
      std::stringstream error;
      error << "{\"status\":\"error\",\"message\":\"Unable to update public name: "
            << json_escape(queue_error) << ".\"}";
      text_reply(rep, reply::bad_request, error.str(), "application/json");
      return;
    }

    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"message\":\"Name update queued and broadcast. The new name will show after a block includes this record.\",";
    ss << "\"name\":\"" << json_escape(name) << "\",";
    ss << "\"hash\":\"" << json_escape(nameRecord.hash) << "\"}";
    text_reply(rep, reply::accepted, ss.str(), "application/json");
    return;
  }

  if (request_path.find("/api/wallet/send?") == 0
      || (req.method == "POST" && request_path == "/api/wallet/send"))
  {
    CWallet wallet;
    if (wallet.fileExists("wallet.dat") == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"message\":\"Wallet file not found.\"}", "application/json");
      return;
    }

    std::string recipient = submitted_value(req, request_path, "recipient");
    std::string amount_value = submitted_value(req, request_path, "amount");
    double amount = 0.0;
    if (recipient.length() == 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Recipient address is required.\"}", "application/json");
      return;
    }
    if (parse_amount_value(amount_value, amount) == false || amount <= 0.0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Amount must be greater than zero.\"}", "application/json");
      return;
    }

    std::string privateKey;
    std::string publicKey;
    wallet.read(privateKey, publicKey);
    functions.scanChain(publicKey, false);

    double transaction_fee = api_default_transaction_fee();
    double total_debit = amount + transaction_fee;
    if (total_debit > functions.balance)
    {
      std::stringstream error;
      error << "{\"status\":\"error\",\"message\":\"Insufficient balance. Amount plus fee is "
            << total_debit << " SFR.\"}";
      text_reply(rep, reply::bad_request, error.str(), "application/json");
      return;
    }

    CFunctions::record_structure sendRecord;
    sendRecord.network = "main";
    CNetworkTime netTime;
    std::stringstream time_stream;
    time_stream << netTime.getEpoch();
    sendRecord.time = time_stream.str();
    sendRecord.transaction_type = CFunctions::TRANSFER_CURRENCY;
    sendRecord.amount = amount;
    sendRecord.fee = transaction_fee;
    sendRecord.sender_public_key = publicKey;
    sendRecord.recipient_public_key = recipient;
    sendRecord.hash = functions.getRecordHash(sendRecord);

    CECDSACrypto ecdsa;
    std::string signature = "";
    ecdsa.SignMessage(privateKey, sendRecord.hash, signature);
    sendRecord.signature = signature;

    std::string queue_error;
    if (queue_and_broadcast_record(functions, sendRecord, queue_error) == false)
    {
      std::stringstream error;
      error << "{\"status\":\"error\",\"message\":\"Unable to send transfer: "
            << json_escape(queue_error) << ".\"}";
      text_reply(rep, reply::bad_request, error.str(), "application/json");
      return;
    }

    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"message\":\"Transfer request queued and broadcast.\",";
    ss << "\"amount\":\"" << amount << "\",";
    ss << "\"fee\":\"" << transaction_fee << "\",";
    ss << "\"total\":\"" << total_debit << "\",";
    ss << "\"recipient\":\"" << json_escape(recipient) << "\",";
    ss << "\"hash\":\"" << json_escape(sendRecord.hash) << "\"}";
    text_reply(rep, reply::accepted, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/network/users")
  {
    text_reply(rep, reply::ok, network_users_json(blockDB), "application/json");
    return;
  }

  if (request_path == "/api/mempool")
  {
    text_reply(rep, reply::ok, mempool_json(), "application/json");
    return;
  }

  if (request_path == "/api/blockchain/recent")
  {
    text_reply(rep, reply::ok, recent_blockchain_json(blockDB), "application/json");
    return;
  }

  if (request_path == "/api/time")
  {
    CNetworkTime netTime;
    long epoch = netTime.getEpoch();
    std::stringstream ss;
    ss << "{\"epoch\":\"" << epoch << "\",";
    ss << "\"local_epoch\":\"" << netTime.getLocalEpoch() << "\",";
    ss << "\"offset\":\"" << netTime.getOffset() << "\",";
    ss << "\"slot\":\"" << (epoch / 15) << "\",";
    ss << "\"block_interval\":\"15\"}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/blocks/first")
  {
    long firstBlockId = blockDB.getFirstBlockId();
    if (firstBlockId < 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }

    CFunctions::block_structure block = blockDB.getBlock(firstBlockId);
    text_reply(rep, reply::ok, functions.blockJSON(block), "application/json");
    return;
  }

  if (request_path == "/api/blocks/latest")
  {
    long latestBlockId = blockDB.getLatestBlockId();
    if (latestBlockId < 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }

    CFunctions::block_structure block = blockDB.getBlock(latestBlockId);
    text_reply(rep, reply::ok, functions.blockJSON(block), "application/json");
    return;
  }

  if (request_path.find("/api/blocks/submit?") == 0
      || (req.method == "POST" && request_path == "/api/blocks/submit"))
  {
    std::string blockJson = submitted_value(req, request_path, "block");
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(blockJson);
    bool accepted = false;
    bool added = false;
    for (int i = 0; i < blocks.size(); ++i)
    {
      CFunctions::block_structure block = blocks.at(i);
      if (block.number > 0)
      {
        bool alreadyStored = blockDB.getBlockByHash(block.hash).number > 0;
        bool blockAdded = blockDB.AddBlock(block);
        if (alreadyStored == false && blockAdded && blockDB.getFirstBlockId() == -1 && block.previous_block_id <= 0)
        {
          blockDB.setFirstBlockId(block.number);
        }
        if (alreadyStored == false && blockAdded)
        {
          long connectedLatestBlockId = blockDB.getConnectedLatestBlockId();
          if (connectedLatestBlockId > -1)
          {
            blockDB.setLatestBlockId(connectedLatestBlockId);
          }
        }
        accepted = alreadyStored || blockAdded || accepted;
        added = (alreadyStored == false && blockAdded) || added;
      }
    }

    if (added)
    {
      blockDB.rebuildBestChainIndex();
    }

    text_reply(rep, accepted ? reply::accepted : reply::bad_request, accepted ? "accepted" : "invalid block", "text/plain");
    return;
  }

  std::string blockPrefix = "/api/blocks/";
  if (request_path.find(blockPrefix) == 0
      && request_path.find("/api/blocks/after/") != 0
      && request_path.find("/api/blocks/after-hash/") != 0)
  {
    std::string blockNumberString = request_path.substr(blockPrefix.length());
    long blockNumber = ::atol(blockNumberString.c_str());
    CFunctions::block_structure block = blockDB.getBlock(blockNumber);
    if (block.number <= 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }

    text_reply(rep, reply::ok, functions.blockJSON(block), "application/json");
    return;
  }

  std::string blockAfterPrefix = "/api/blocks/after/";
  if (request_path.find(blockAfterPrefix) == 0)
  {
    std::string blockNumberString = request_path.substr(blockAfterPrefix.length());
    long blockNumber = ::atol(blockNumberString.c_str());
    CFunctions::block_structure block = blockDB.getBlock(blockNumber);
    if (block.number <= 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }

    CFunctions::block_structure nextBlock = blockDB.getNextBlock(block);
    if (nextBlock.number <= 0 || nextBlock.number == block.number)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }

    text_reply(rep, reply::ok, functions.blockJSON(nextBlock), "application/json");
    return;
  }

  std::string blockAfterHashPrefix = "/api/blocks/after-hash/";
  if (request_path.find(blockAfterHashPrefix) == 0)
  {
    std::string blockHash = request_path.substr(blockAfterHashPrefix.length());
    CFunctions::block_structure block = blockDB.getBlockByHash(blockHash);
    if (block.number <= 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }

    CFunctions::block_structure nextBlock = blockDB.getNextBlockByHash(block);
    if (nextBlock.number <= 0 || nextBlock.number == block.number)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }

    text_reply(rep, reply::ok, functions.blockJSON(nextBlock), "application/json");
    return;
  }

  if (request_path.find("/api/records/submit?") == 0
      || (req.method == "POST" && request_path == "/api/records/submit"))
  {
    std::string recordJson = submitted_value(req, request_path, "record");
    CFunctions::record_structure record = functions.parseRecordJson(recordJson);
    if (record.sender_public_key.empty() && record.recipient_public_key.empty())
    {
      text_reply(rep, reply::bad_request, "invalid record", "text/plain");
      return;
    }

    functions.addToQueue(record);
    text_reply(rep, reply::accepted, "accepted", "text/plain");
    return;
  }

/*
  // Request path must be absolute and not contain "..".
  if (request_path.empty() || request_path[0] != '/'
      || request_path.find("..") != std::string::npos)
  {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // If path ends in slash (i.e. is a directory) then add "index.html".
  if (request_path[request_path.size() - 1] == '/')
  {
    request_path += "index.html";
  }

  // Determine the file extension.
  std::size_t last_slash_pos = request_path.find_last_of("/");
  std::size_t last_dot_pos = request_path.find_last_of(".");
  std::string extension;
  if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos)
  {
    extension = request_path.substr(last_dot_pos + 1);
  }

  // Open the file to send back.
  std::string full_path = doc_root_ + request_path;
  std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
  if (!is)
  {
    rep = reply::stock_reply(reply::not_found);
    return;
  }

  // Fill out the reply to be sent to the client.
  rep.status = reply::ok;
  char buf[512];
  while (is.read(buf, sizeof(buf)).gcount() > 0)
    rep.content.append(buf, is.gcount());
  rep.headers.resize(2);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = boost::lexical_cast<std::string>(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = mime_types::extension_to_type(extension);
*/
  std::string line = "<br>";
  std::string extension = "html"; //  "txt";

  std::stringstream ss;
  std::string responseContent = "";
  ss << "Safire Client v0.0.1. " << line;
  ss << line;

  CWallet wallet;
  std::string privateKey;
  std::string publicKey;
  bool e = wallet.fileExists("wallet.dat");
  if(e != 0){
        wallet.read(privateKey, publicKey);
        ss << "Public Key: " << publicKey << line;
  }

  ss << "Balance " << 0 << line;
  ss << "My Transactions " << 0 << line;

  ss << line;
  ss << "Network Users " << 0 << line;
  ss << "Pending Users " << 0 << line;

  CNetworkTime netTime;
  ss << "Time " << netTime.getEpoch() << line;  
  // Get Block time 
  ss << "Block Time " << 0 << line;
  ss << "Block Creator Key " << "" << line; 

  ss << line;
  // Blocks
  ss << "Blocks Stored " << 0 << line;
  ss << "Network Transactions " << 0 << line;
  ss << "Pending Transactions " << 0 << line;

  // Availability
  // network reward
  // currency outstanding.

  ss << "Request " << request_path << line;
  
  responseContent = ss.str();

  rep.status = reply::ok;
  rep.content.append(responseContent.c_str(), responseContent.length());
  rep.headers.resize(2);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = boost::lexical_cast<std::string>(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = mime_types::extension_to_type(extension);

}

bool request_handler::url_decode(const std::string& in, std::string& out)
{
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 3 <= in.size())
      {
        int value;
        std::istringstream is(in.substr(i + 1, 2));
        if (is >> std::hex >> value)
        {
          out += static_cast<char>(value);
          i += 2;
        }
        else
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }
    else if (in[i] == '+')
    {
      out += ' ';
    }
    else
    {
      out += in[i];
    }
  }
  return true;
}

} // namespace server3
} // namespace http
