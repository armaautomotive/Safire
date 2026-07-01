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
#include <cstdio>
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
#include "functions/ledgerstate.h"
#include "functions/selector.h"
#include "network/localpeerclient.h"
#include "network/natmapper.h"
#include "network/relayclient.h"
#include "networkconfig.h"

#include "networktime.h"

namespace http {
namespace server3 {

namespace {

const double API_DEFAULT_TRANSACTION_FEE = 0.0;
const char* HANDOFF_FILE = "handoff.dat";

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

std::string sha256_string(const std::string& value)
{
  CECDSACrypto ecdsa;
  char hash[65];
  ecdsa.sha256((char*)value.c_str(), hash);
  return std::string(hash);
}

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

std::string json_field(const std::string& json, const std::string& key)
{
  CFunctions functions;
  std::string token = "\"" + key + "\":\"";
  return functions.parseSectionString(json, token, "\"");
}

long json_field_long(const std::string& json, const std::string& key)
{
  CFunctions functions;
  std::string token = "\"" + key + "\":\"";
  return functions.parseSectionLong(json, token, "\"");
}

std::string submitted_value_any(const request& req, const std::string& request_path, const std::string& key)
{
  if (req.method == "POST" && !req.body.empty() &&
      req.body.find("\"" + key + "\":\"") != std::string::npos)
  {
    return json_field(req.body, key);
  }
  return submitted_value(req, request_path, key);
}

std::string handoff_hash_seed(
  long block_number,
  const std::string& block_hash,
  const std::string& parent_hash,
  const std::string& creator,
  long next_slot,
  const std::string& next_creator,
  const std::string& active_member_set_hash)
{
  std::stringstream seed;
  seed << block_number << "|"
       << block_hash << "|"
       << parent_hash << "|"
       << creator << "|"
       << next_slot << "|"
       << next_creator << "|"
       << active_member_set_hash;
  return sha256_string(seed.str());
}

std::string read_handoff_file()
{
  std::ifstream infile(HANDOFF_FILE);
  if (!infile.good())
  {
    return "";
  }

  std::stringstream ss;
  ss << infile.rdbuf();
  return ss.str();
}

bool write_handoff_file(const std::string& json)
{
  std::ofstream outfile(HANDOFF_FILE);
  if (!outfile.good())
  {
    return false;
  }
  outfile << json;
  outfile.close();
  return true;
}

std::string handoff_latest_json(CBlockDB& block_db)
{
  std::string handoff = read_handoff_file();
  if (handoff.length() == 0)
  {
    return "{\"status\":\"empty\",\"message\":\"No handoff message has been received.\"}";
  }

  std::string block_hash = json_field(handoff, "block_hash");
  CFunctions::block_structure block = block_db.getBlockByHash(block_hash);

  std::stringstream ss;
  ss << "{\"status\":\"ok\",";
  ss << "\"block_known\":\"" << (block.number > 0 ? "yes" : "no") << "\",";
  ss << "\"handoff\":" << handoff << "}";
  return ss.str();
}

std::string submit_handoff_json(CBlockDB& block_db, const std::string& handoff, bool& accepted)
{
  accepted = false;
  if (handoff.length() == 0)
  {
    return "{\"status\":\"error\",\"message\":\"Missing handoff message.\"}";
  }

  long block_number = json_field_long(handoff, "block_number");
  std::string block_hash = json_field(handoff, "block_hash");
  std::string parent_hash = json_field(handoff, "parent_hash");
  std::string creator = json_field(handoff, "creator");
  long next_slot = json_field_long(handoff, "next_slot");
  std::string next_creator = json_field(handoff, "next_creator");
  std::string active_member_set_hash = json_field(handoff, "active_member_set_hash");
  std::string handoff_hash = json_field(handoff, "handoff_hash");
  std::string signature = json_field(handoff, "signature");

  if (block_number <= 0 || block_hash.length() == 0 || creator.length() == 0 ||
      next_slot <= 0 || handoff_hash.length() == 0 || signature.length() == 0)
  {
    return "{\"status\":\"error\",\"message\":\"Incomplete handoff message.\"}";
  }

  std::string expected_hash = handoff_hash_seed(
    block_number,
    block_hash,
    parent_hash,
    creator,
    next_slot,
    next_creator,
    active_member_set_hash);
  if (expected_hash.compare(handoff_hash) != 0)
  {
    return "{\"status\":\"invalid\",\"message\":\"Handoff hash does not match message fields.\"}";
  }

  CECDSACrypto ecdsa;
  if (ecdsa.VerifyMessageCompressed(handoff_hash, signature, creator) != 1)
  {
    return "{\"status\":\"invalid\",\"message\":\"Handoff signature is invalid.\"}";
  }

  CFunctions::block_structure block = block_db.getBlockByHash(block_hash);
  bool block_known = block.number > 0;
  bool block_matches = block_known &&
    block.number == block_number &&
    block.previous_block_hash.compare(parent_hash) == 0 &&
    block.creator_key.compare(creator) == 0;

  if (write_handoff_file(handoff) == false)
  {
    return "{\"status\":\"error\",\"message\":\"Unable to store handoff message.\"}";
  }

  accepted = true;
  std::stringstream ss;
  ss << "{\"status\":\"accepted\",";
  ss << "\"verification\":\"hash_signature_valid\",";
  ss << "\"block_known\":\"" << (block_known ? "yes" : "no") << "\",";
  ss << "\"block_matches\":\"" << (block_matches ? "yes" : (block_known ? "no" : "unknown")) << "\",";
  ss << "\"block_number\":\"" << block_number << "\",";
  ss << "\"next_slot\":\"" << next_slot << "\",";
  ss << "\"next_creator\":\"" << json_escape(next_creator) << "\"}";
  return ss.str();
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

std::string configured_exchange_api_key()
{
  std::ifstream infile(API_SETTINGS_FILE);
  std::string line;
  while (std::getline(infile, line))
  {
    std::size_t start = line.find("exchange_api_key:");
    if (start == 0)
    {
      return line.substr(17);
    }
  }
  return "";
}

bool exchange_api_authorized(const request& req, const std::string& request_path)
{
  std::string configured_key = configured_exchange_api_key();
  if (configured_key.length() == 0)
  {
    return false;
  }
  std::string submitted_key = submitted_value_any(req, request_path, "api_key");
  return submitted_key.length() > 0 && submitted_key.compare(configured_key) == 0;
}

std::string configured_admin_api_key()
{
  std::ifstream infile(API_SETTINGS_FILE);
  std::string line;
  while (std::getline(infile, line))
  {
    std::size_t start = line.find("admin_api_key:");
    if (start == 0)
    {
      return line.substr(14);
    }
  }
  return "";
}

bool admin_api_authorized(const request& req, const std::string& request_path)
{
  std::string configured_key = configured_admin_api_key();
  if (configured_key.length() == 0)
  {
    return false;
  }
  std::string submitted_key = submitted_value_any(req, request_path, "api_key");
  return submitted_key.length() > 0 && submitted_key.compare(configured_key) == 0;
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

const CLedgerState::member_state* ledger_member_by_key(const CLedgerState::state& state, const std::string& public_key)
{
  for (int i = 0; i < state.members.size(); ++i)
  {
    if (state.members.at(i).public_key.compare(public_key) == 0)
    {
      return &state.members.at(i);
    }
  }
  return 0;
}

std::vector<CFunctions::record_structure> accepted_membership_records(CBlockDB& block_db)
{
  std::vector<CFunctions::record_structure> members;
  CLedgerState::state ledger_state = CLedgerState::build(block_db);
  for (int i = 0; i < ledger_state.members.size(); ++i)
  {
    CFunctions::record_structure member;
    member.transaction_type = CFunctions::JOIN_NETWORK;
    member.sender_public_key = ledger_state.members.at(i).public_key;
    member.name = ledger_state.members.at(i).name;
    members.push_back(member);
  }
  return members;
}

std::map<std::string, std::string> accepted_member_names(CBlockDB& block_db)
{
  return CLedgerState::build(block_db).names;
}

std::vector<CFunctions::record_structure> active_membership_records(CBlockDB& block_db)
{
  std::vector<CFunctions::record_structure> active_members;
  CLedgerState::state ledger_state = CLedgerState::build(block_db);
  for (int i = 0; i < ledger_state.members.size(); ++i)
  {
    if (ledger_state.members.at(i).active)
    {
      CFunctions::record_structure member;
      member.transaction_type = CFunctions::JOIN_NETWORK;
      member.sender_public_key = ledger_state.members.at(i).public_key;
      member.name = ledger_state.members.at(i).name;
      active_members.push_back(member);
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

struct exchange_record_location
{
  bool found;
  bool pending;
  long block_number;
  int record_index;
  long confirmations;
  CFunctions::block_structure block;
  CFunctions::record_structure record;
};

std::string path_argument_after_prefix(const std::string& request_path, const std::string& prefix)
{
  std::string value = request_path.substr(prefix.length());
  std::size_t query = value.find("?");
  if (query != std::string::npos)
  {
    value = value.substr(0, query);
  }
  return value;
}

exchange_record_location empty_exchange_record_location()
{
  exchange_record_location location;
  location.found = false;
  location.pending = false;
  location.block_number = -1;
  location.record_index = -1;
  location.confirmations = 0;
  return location;
}

std::string exchange_record_json(const exchange_record_location& location,
                                 const std::map<std::string, std::string>& names)
{
  std::stringstream ss;
  ss << "{";
  ss << "\"hash\":\"" << json_escape(location.record.hash) << "\",";
  ss << "\"status\":\"" << (location.pending ? "pending" : "accepted") << "\",";
  ss << "\"confirmations\":\"" << location.confirmations << "\",";
  ss << "\"block\":\"" << location.block_number << "\",";
  ss << "\"index\":\"" << location.record_index << "\",";
  ss << "\"block_hash\":\"" << json_escape(location.block.hash) << "\",";
  ss << "\"type\":\"" << record_type_name(location.record.transaction_type) << "\",";
  ss << "\"network\":\"" << json_escape(location.record.network) << "\",";
  ss << "\"time\":\"" << json_escape(location.record.time) << "\",";
  if (location.record.nonce > 0)
  {
    ss << "\"nonce\":\"" << location.record.nonce << "\",";
  }
  ss << "\"amount\":\"" << location.record.amount << "\",";
  ss << "\"fee\":\"" << location.record.fee << "\",";
  ss << "\"from_key\":\"" << json_escape(location.record.sender_public_key) << "\",";
  ss << "\"from_name\":\"" << json_escape(name_for_key(names, location.record.sender_public_key)) << "\",";
  ss << "\"to_key\":\"" << json_escape(location.record.recipient_public_key) << "\",";
  ss << "\"to_name\":\"" << json_escape(name_for_key(names, location.record.recipient_public_key)) << "\",";
  ss << "\"name\":\"" << json_escape(location.record.name) << "\",";
  ss << "\"value\":\"" << json_escape(location.record.value) << "\"";
  ss << "}";
  return ss.str();
}

exchange_record_location find_exchange_record(CBlockDB& block_db, const std::string& record_hash)
{
  exchange_record_location location = empty_exchange_record_location();
  if (record_hash.length() == 0)
  {
    return location;
  }

  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  if (first_block_id >= 0 && latest_block_id >= 0)
  {
    CFunctions::block_structure block = block_db.getBlock(first_block_id);
    std::set<std::string> accepted_hashes;
    int guard = 0;
    while (block.number > 0 && guard < 100000)
    {
      for (int i = 0; i < block.records.size(); ++i)
      {
        CFunctions::record_structure record = block.records.at(i);
        if (record.hash.length() > 0 && accepted_hashes.find(record.hash) != accepted_hashes.end())
        {
          continue;
        }
        if (record.hash.length() > 0)
        {
          accepted_hashes.insert(record.hash);
        }
        if (record.hash.compare(record_hash) == 0)
        {
          location.found = true;
          location.pending = false;
          location.block_number = block.number;
          location.record_index = i;
          location.confirmations = latest_block_id >= block.number ? latest_block_id - block.number + 1 : 0;
          location.block = block;
          location.record = record;
          return location;
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
  }

  CFunctions functions;
  std::vector<CFunctions::record_structure> pending_records = functions.peekQueueRecords();
  for (int i = 0; i < pending_records.size(); ++i)
  {
    CFunctions::record_structure record = pending_records.at(i);
    if (record.hash.compare(record_hash) == 0)
    {
      location.found = true;
      location.pending = true;
      location.block_number = -1;
      location.record_index = i;
      location.confirmations = 0;
      location.record = record;
      return location;
    }
  }

  return location;
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
    if (record.nonce > 0)
    {
      ss << "\"nonce\":\"" << record.nonce << "\",";
    }
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

  std::vector<CLocalPeerClient::peer_status> peers = CLocalPeerClient::getPeerStatuses(true);
  CLocalPeerClient::peer_status best_peer;
  bool has_best_peer = best_peer_status(peers, best_peer);
  CFunctions::block_structure local_tip = block_db.getBlock(latest_block_id);
  bool peer_tip_matches_local_tip = false;
  bool peer_tip_matches_local_ancestor = false;
  if (has_best_peer && best_peer.latestBlockHash.length() > 0)
  {
    if (best_peer.latestBlockId == local_tip.number &&
        best_peer.latestBlockHash.compare(local_tip.hash) == 0)
    {
      peer_tip_matches_local_tip = true;
    }
    else if (best_peer.latestBlockId < local_tip.number)
    {
      CFunctions::block_structure local_peer_tip = block_db.getBlock(best_peer.latestBlockId);
      if (local_peer_tip.number == best_peer.latestBlockId &&
          best_peer.latestBlockHash.compare(local_peer_tip.hash) == 0)
      {
        peer_tip_matches_local_ancestor = true;
      }
    }
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
      else if (peer_tip_matches_local_tip ||
               (peer_tip_matches_local_ancestor && current_block.number <= best_peer.latestBlockId))
      {
        peer_hash = current_block.hash;
        network_status = "match";
      }
      else if (current_block.number == best_peer.latestBlockId && best_peer.latestBlockHash.length() > 0)
      {
        peer_hash = best_peer.latestBlockHash;
        network_status = current_block.hash.compare(peer_hash) == 0 ? "match" : "mismatch";
      }
      else
      {
        network_status = "unknown";
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
    ss << "\"records_merkle_root\":\"" << json_escape(current_block.records_merkle_root) << "\",";
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
      if (record.nonce > 0)
      {
        ss << "\"nonce\":\"" << record.nonce << "\",";
      }
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
  return CLedgerState::build(block_db).balances;
}

double pending_wallet_balance_delta(const std::string& public_key,
                                    const std::vector<CFunctions::record_structure>& records,
                                    int& pending_record_count)
{
  pending_record_count = 0;
  double delta = 0.0;

  for (int i = 0; i < records.size(); ++i)
  {
    CFunctions::record_structure record = records.at(i);
    bool touches_wallet = record.sender_public_key.compare(public_key) == 0 ||
                          record.recipient_public_key.compare(public_key) == 0;
    if (!touches_wallet)
    {
      continue;
    }

    pending_record_count++;
    if (record.transaction_type == CFunctions::TRANSFER_CURRENCY)
    {
      bool sent_by_wallet = record.sender_public_key.compare(public_key) == 0;
      bool received_by_wallet = record.recipient_public_key.compare(public_key) == 0;
      if (sent_by_wallet && received_by_wallet)
      {
        delta -= record.fee;
      }
      else if (sent_by_wallet)
      {
        delta -= record.amount + record.fee;
      }
      else if (received_by_wallet)
      {
        delta += record.amount;
      }
    }
  }
  return delta;
}

long next_transfer_nonce(CBlockDB& block_db, const std::string& public_key)
{
  CLedgerState::state ledger_state = CLedgerState::build(block_db);
  long nonce = ledger_state.nonces[public_key];

  CFunctions functions;
  std::vector<CFunctions::record_structure> records = functions.peekQueueRecords();
  for (int i = 0; i < records.size(); ++i)
  {
    CFunctions::record_structure record = records.at(i);
    if (record.transaction_type == CFunctions::TRANSFER_CURRENCY &&
        record.sender_public_key.compare(public_key) == 0 &&
        record.nonce > nonce)
    {
      nonce = record.nonce;
    }
  }
  return nonce + 1;
}

bool selected_wallet_keys(const request& req,
                          const std::string& request_path,
                          CWallet& wallet,
                          std::string& private_key,
                          std::string& public_key)
{
  std::string wallet_id = submitted_value_any(req, request_path, "wallet_id");
  if (wallet_id.length() == 0)
  {
    wallet_id = submitted_value_any(req, request_path, "address");
  }
  if (wallet_id.length() > 0)
  {
    if (wallet.readAccount(wallet_id, private_key, public_key))
    {
      return true;
    }
    return wallet.read(private_key, public_key);
  }
  return wallet.read(private_key, public_key);
}

double accepted_member_supply(CBlockDB& block_db)
{
  return CLedgerState::build(block_db).ledger_balance_total;
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

std::string peers_json(CBlockDB& block_db)
{
  std::vector<CLocalPeerClient::peer_status> peers = CLocalPeerClient::getPeerStatuses();
  std::string advertisedPeer = CLocalPeerClient::getAdvertisedPeer();
  long firstBlockId = block_db.getFirstBlockId();
  long latestBlockId = block_db.getLatestBlockId();
  CFunctions::block_structure firstBlock = block_db.getBlock(firstBlockId);
  CFunctions::block_structure latestBlock = block_db.getBlock(latestBlockId);
  CNetworkConfig config = CNetworkConfig::load();
  bool selfGenesisMatch = config.genesisMatches(firstBlockId, firstBlock.hash);
  CWallet wallet;
  std::string privateKey;
  std::string publicKey;
  std::string publicName;
  if (wallet.read(privateKey, publicKey))
  {
    std::map<std::string, std::string> names = accepted_member_names(block_db);
    publicName = name_for_key(names, publicKey);
  }
  std::stringstream ss;
  ss << "{\"protocol_version\":\"" << CLocalPeerClient::PROTOCOL_VERSION << "\",";
  ss << "\"self_url\":\"" << json_escape(advertisedPeer) << "\",";
  ss << "\"peers\":[";
  bool wrotePeer = false;
  if (advertisedPeer.length() > 0)
  {
    ss << "{";
    ss << "\"url\":\"" << json_escape(advertisedPeer) << "\",";
    ss << "\"public_key\":\"" << json_escape(publicKey) << "\",";
    ss << "\"public_name\":\"" << json_escape(publicName) << "\",";
    ss << "\"latest_block_id\":\"" << latestBlockId << "\",";
    ss << "\"latest_block_hash\":\"" << json_escape(latestBlock.hash) << "\",";
    ss << "\"genesis_match\":\"" << (selfGenesisMatch ? "yes" : "no") << "\",";
    ss << "\"reachable\":\"" << (latestBlockId >= 0 ? "yes" : "no") << "\",";
    ss << "\"last_success_epoch\":\"0\",";
    ss << "\"first_failure_epoch\":\"0\",";
    ss << "\"score\":\"100\"";
    ss << "}";
    wrotePeer = true;
  }
  for (int i = 0; i < peers.size(); ++i)
  {
    if (advertisedPeer.length() > 0 && peers.at(i).url.compare(advertisedPeer) == 0)
    {
      continue;
    }
    if (wrotePeer)
    {
      ss << ",";
    }
    ss << "{";
    ss << "\"url\":\"" << json_escape(peers.at(i).url) << "\",";
    ss << "\"public_key\":\"" << json_escape(peers.at(i).publicKey) << "\",";
    ss << "\"public_name\":\"" << json_escape(peers.at(i).publicName) << "\",";
    ss << "\"latest_block_id\":\"" << peers.at(i).latestBlockId << "\",";
    ss << "\"latest_block_hash\":\"" << peers.at(i).latestBlockHash << "\",";
    ss << "\"genesis_match\":\"" << (peers.at(i).genesisMatch ? "yes" : "no") << "\",";
    ss << "\"reachable\":\"" << (peers.at(i).reachable ? "yes" : "no") << "\",";
    ss << "\"last_success_epoch\":\"" << peers.at(i).lastSuccessEpoch << "\",";
    ss << "\"first_failure_epoch\":\"" << peers.at(i).firstFailureEpoch << "\",";
    ss << "\"score\":\"" << peers.at(i).score << "\"";
    ss << "}";
    wrotePeer = true;
  }
  ss << "]}";
  return ss.str();
}

std::string sync_peers_json()
{
  std::vector<std::string> localPeers = CLocalPeerClient::getPeers();
  CLocalPeerClient::syncNetworkTime();
  CNetworkTime netTime;

  std::stringstream ss;
  ss << "{\"status\":\"ok\",";
  ss << "\"network_time_offset\":\"" << netTime.getOffset() << "\",";
  ss << "\"peers\":[";
  for (int i = 0; i < localPeers.size(); ++i)
  {
    CBlockDB blockDB;
    long before = blockDB.getLatestBlockId();
    long peerBefore = CLocalPeerClient::getPeerLatestBlockId(localPeers.at(i));
    bool pulled = CLocalPeerClient::syncFromPeer(localPeers.at(i));
    CLocalPeerClient::push_result pushResult = CLocalPeerClient::pushToPeerDetailed(localPeers.at(i));
    long after = blockDB.getLatestBlockId();
    long peerAfter = CLocalPeerClient::getPeerLatestBlockId(localPeers.at(i));

    if (i > 0)
    {
      ss << ",";
    }
    ss << "{";
    ss << "\"url\":\"" << json_escape(localPeers.at(i)) << "\",";
    ss << "\"local_latest_before\":\"" << before << "\",";
    ss << "\"local_latest_after\":\"" << after << "\",";
    ss << "\"peer_latest_before\":\"" << peerBefore << "\",";
    ss << "\"peer_latest_after\":\"" << peerAfter << "\",";
    ss << "\"pulled\":\"" << (pulled ? "yes" : "no") << "\",";
    ss << "\"candidate_blocks\":\"" << pushResult.candidateBlocks << "\",";
    ss << "\"pushed_blocks\":\"" << pushResult.pushedBlocks << "\",";
    ss << "\"failed_block\":\"" << pushResult.failedBlockId << "\",";
    ss << "\"response\":\"" << json_escape(pushResult.response) << "\"";
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
    if (item.record.nonce > 0)
    {
      ss << "\"nonce\":\"" << item.record.nonce << "\",";
    }
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
                             const std::vector<CLocalPeerClient::peer_status>& local_peers);

bool local_chain_matches_peer(CBlockDB& block_db,
                              const CLocalPeerClient::peer_status& peer);

bool synced_with_peer_latest(CBlockDB& block_db,
                             long latest_block_id,
                             const std::vector<CLocalPeerClient::peer_status>& local_peers);

std::string exchange_status_json(CBlockDB& block_db)
{
  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  long block_count = connected_block_count(block_db);
  CFunctions::block_structure first_block = block_db.getBlock(first_block_id);
  CFunctions::block_structure latest_block = block_db.getBlock(latest_block_id);
  CNetworkConfig config = CNetworkConfig::load();
  CNetworkTime net_time;
  std::vector<CLocalPeerClient::peer_status> local_peers = CLocalPeerClient::getPeerStatuses(true);
  CLocalPeerClient::peer_status best_peer;
  bool has_best_peer = best_peer_status(local_peers, best_peer);
  bool peer_chain_match = has_best_peer ? local_chain_matches_peer(block_db, best_peer) : true;
  bool peer_sync = synced_with_peer_latest(block_db, latest_block_id, local_peers);
  CLedgerState::state state = CLedgerState::build(block_db);

  std::string public_key = "";
  double wallet_balance = 0.0;
  CWallet wallet;
  if (wallet.fileExists("wallet.dat"))
  {
    std::string private_key;
    wallet.read(private_key, public_key);
    wallet_balance = state.balances[public_key];
  }

  std::stringstream ss;
  ss << "{\"status\":\"ok\",";
  ss << "\"role\":\"exchange_node\",";
  ss << "\"public_key\":\"" << json_escape(public_key) << "\",";
  ss << "\"wallet_balance\":\"" << wallet_balance << "\",";
  ss << "\"first_block_id\":\"" << first_block_id << "\",";
  ss << "\"first_block_hash\":\"" << json_escape(first_block.hash) << "\",";
  ss << "\"latest_block_id\":\"" << latest_block_id << "\",";
  ss << "\"latest_block_hash\":\"" << json_escape(latest_block.hash) << "\",";
  ss << "\"latest_block_time\":\"" << json_escape(latest_block.time) << "\",";
  ss << "\"block_count\":\"" << block_count << "\",";
  ss << "\"issued_supply\":\"" << state.issued_supply << "\",";
  ss << "\"ledger_balance_total\":\"" << state.ledger_balance_total << "\",";
  ss << "\"user_count\":\"" << state.members.size() << "\",";
  ss << "\"genesis_match\":\"" << (config.genesisMatches(first_block_id, first_block.hash) ? "yes" : "no") << "\",";
  ss << "\"peer_sync\":\"" << (peer_sync ? "yes" : "no") << "\",";
  ss << "\"peer_chain_match\":\"" << (peer_chain_match ? "yes" : "no") << "\",";
  ss << "\"peer_latest_block_id\":\"" << (has_best_peer ? best_peer.latestBlockId : -1) << "\",";
  ss << "\"peer_latest_block_hash\":\"" << json_escape(has_best_peer ? best_peer.latestBlockHash : "") << "\",";
  ss << "\"sync_progress\":\"" << sync_progress_percent(first_block_id, latest_block_id, local_peers) << "\",";
  ss << "\"network_time_offset\":\"" << net_time.getOffset() << "\",";
  ss << "\"local_peers\":\"" << local_peers.size() << "\",";
  ss << "\"withdrawal_api\":\"" << (configured_exchange_api_key().length() > 0 ? "configured" : "disabled") << "\"";
  ss << "}";
  return ss.str();
}

std::string exchange_deposits_json(CBlockDB& block_db, const std::string& address)
{
  if (address.length() == 0)
  {
    return "{\"status\":\"error\",\"message\":\"Deposit address is required.\",\"deposits\":[]}";
  }

  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  std::map<std::string, std::string> names = accepted_member_names(block_db);
  std::deque<exchange_record_location> deposits;
  const int limit = 500;

  if (first_block_id >= 0 && latest_block_id >= 0)
  {
    CFunctions::block_structure block = block_db.getBlock(first_block_id);
    std::set<std::string> accepted_hashes;
    int guard = 0;
    while (block.number > 0 && guard < 100000)
    {
      for (int i = 0; i < block.records.size(); ++i)
      {
        CFunctions::record_structure record = block.records.at(i);
        if (record.hash.length() > 0 && accepted_hashes.find(record.hash) != accepted_hashes.end())
        {
          continue;
        }
        if (record.hash.length() > 0)
        {
          accepted_hashes.insert(record.hash);
        }
        if (record.transaction_type == CFunctions::TRANSFER_CURRENCY &&
            record.recipient_public_key.compare(address) == 0)
        {
          exchange_record_location location = empty_exchange_record_location();
          location.found = true;
          location.pending = false;
          location.block_number = block.number;
          location.record_index = i;
          location.confirmations = latest_block_id >= block.number ? latest_block_id - block.number + 1 : 0;
          location.block = block;
          location.record = record;
          deposits.push_back(location);
          if (deposits.size() > limit)
          {
            deposits.pop_front();
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
  }

  std::stringstream ss;
  ss << "{\"status\":\"ok\",";
  ss << "\"address\":\"" << json_escape(address) << "\",";
  ss << "\"limit\":\"" << limit << "\",";
  ss << "\"deposits\":[";
  for (int i = 0; i < deposits.size(); ++i)
  {
    if (i > 0)
    {
      ss << ",";
    }
    ss << exchange_record_json(deposits.at(i), names);
  }
  ss << "]}";
  return ss.str();
}

std::string wallet_accounts_json(CBlockDB& block_db)
{
  CWallet wallet;
  std::vector<CWallet::account> accounts = wallet.listAccounts();
  std::string active_id = wallet.activeAccountId();
  CLedgerState::state chain_state = CLedgerState::build(block_db);

  std::stringstream ss;
  ss << "{\"status\":\"ok\",";
  ss << "\"active_wallet_id\":\"" << json_escape(active_id) << "\",";
  ss << "\"accounts\":[";
  for (int i = 0; i < accounts.size(); ++i)
  {
    if (i > 0)
    {
      ss << ",";
    }
    CWallet::account account = accounts.at(i);
    const CLedgerState::member_state* member = ledger_member_by_key(chain_state, account.public_key);
    bool joined = member != 0;
    bool activeHeartbeat = member != 0 && member->active;
    long lastHeartbeatBlock = member != 0 ? member->last_heartbeat_block : -1;
    ss << "{";
    ss << "\"wallet_id\":\"" << json_escape(account.id) << "\",";
    ss << "\"label\":\"" << json_escape(account.label) << "\",";
    ss << "\"public_key\":\"" << json_escape(account.public_key) << "\",";
    ss << "\"public_name\":\"" << json_escape(name_for_key(chain_state.names, account.public_key)) << "\",";
    ss << "\"active\":\"" << (account.id.compare(active_id) == 0 ? "yes" : "no") << "\",";
    ss << "\"balance\":\"" << chain_state.balances[account.public_key] << "\",";
    ss << "\"joined\":\"" << (joined ? "yes" : "no") << "\",";
    ss << "\"active_heartbeat\":\"" << (activeHeartbeat ? "yes" : "no") << "\",";
    ss << "\"last_heartbeat_block\":\"" << lastHeartbeatBlock << "\"";
    ss << "}";
  }
  ss << "]}";
  return ss.str();
}

std::string lower_ascii(std::string value)
{
  for (int i = 0; i < value.length(); ++i)
  {
    value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
  }
  return value;
}

std::string admin_page_html()
{
  return R"HTML(<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Safire Admin</title>
  <style>
    :root { color-scheme: light; --ink:#0e2930; --muted:#637b82; --line:#d9e7ea; --panel:#ffffff; --accent:#087987; --bg:#eef6f8; }
    * { box-sizing: border-box; }
    body { margin:0; font-family:-apple-system,BlinkMacSystemFont,"Helvetica Neue",Arial,sans-serif; color:var(--ink); background:linear-gradient(135deg,#f7fbff,#eaf7f2); }
    header { padding:22px 28px; border-bottom:1px solid var(--line); background:rgba(255,255,255,.82); position:sticky; top:0; z-index:2; }
    h1 { margin:0; font-size:28px; }
    main { padding:22px 28px 34px; max-width:1280px; margin:0 auto; }
    .grid { display:grid; grid-template-columns:repeat(4,minmax(170px,1fr)); gap:14px; }
    .panel { background:rgba(255,255,255,.9); border:1px solid var(--line); border-radius:8px; padding:16px; box-shadow:0 8px 24px rgba(20,60,70,.04); }
    .wide { grid-column:span 2; }
    .full { grid-column:1 / -1; }
    h2 { margin:0 0 12px; font-size:18px; }
    .metric { font-size:28px; font-weight:800; letter-spacing:0; }
    .muted { color:var(--muted); }
    .ok { color:#08734f; font-weight:700; }
    .bad { color:#b3261e; font-weight:700; }
    table { width:100%; border-collapse:collapse; font-size:13px; }
    th,td { text-align:left; border-bottom:1px solid #edf3f5; padding:8px 6px; vertical-align:top; }
    code, pre { font-family:Menlo,Consolas,monospace; }
    pre { margin:0; white-space:pre-wrap; overflow:auto; max-height:360px; background:#0d171b; color:#d9f7ef; border-radius:8px; padding:12px; }
    input,button { font:inherit; border-radius:7px; padding:9px 10px; border:1px solid #bfd3d7; }
    input { background:#fff; color:var(--ink); }
    button { background:var(--accent); color:#fff; border-color:var(--accent); font-weight:700; cursor:pointer; }
    button.secondary { background:#edf5f6; color:#17454d; border-color:#c8d9dc; }
    .command { display:grid; grid-template-columns:180px 1fr auto; gap:10px; margin-bottom:12px; }
    .chips { display:flex; flex-wrap:wrap; gap:8px; margin:10px 0 0; }
    .chip { border:1px solid #bfd3d7; background:#edf5f6; color:#17454d; border-radius:999px; padding:6px 10px; cursor:pointer; }
    @media (max-width:900px) { .grid { grid-template-columns:1fr; } .wide { grid-column:auto; } .command { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <header>
    <h1>Safire Admin</h1>
    <div class="muted">Node management console. Use an SSH tunnel or protected network for remote access.</div>
  </header>
  <main>
    <section class="grid">
      <div class="panel"><h2>Latest Block</h2><div id="latest" class="metric">-</div><div id="sync" class="muted">Sync -</div></div>
      <div class="panel"><h2>Wallet</h2><div id="wallet" class="metric">-</div><div id="walletKey" class="muted">-</div></div>
      <div class="panel"><h2>Network</h2><div id="peers" class="metric">-</div><div id="peerSync" class="muted">Peers</div></div>
      <div class="panel"><h2>Supply</h2><div id="supply" class="metric">-</div><div id="ledger" class="muted">Ledger -</div></div>
      <div class="panel wide"><h2>Users</h2><div id="users"></div></div>
      <div class="panel wide"><h2>Peers</h2><div id="peerList"></div></div>
      <div class="panel full">
        <h2>Command</h2>
        <div class="command">
          <input id="key" type="password" placeholder="admin_api_key">
          <input id="cmd" placeholder="status, users, mempool, blockchain, peers, exchange, accounts">
          <button onclick="runCommand()">Run</button>
        </div>
        <div class="chips" id="chips"></div>
        <pre id="output">Configure settings.dat with admin_api_key:&lt;secret&gt; to enable command execution.</pre>
      </div>
    </section>
  </main>
  <script>
    const safeCommands = ["status","network","sync","users","mempool","blockchain","peers","exchange","accounts"];
    const $ = id => document.getElementById(id);
    const short = v => !v ? "-" : (v.length > 22 ? v.slice(0,12) + "..." + v.slice(-6) : v);
    const fmt = v => (v === undefined || v === null || v === "") ? "-" : String(Number(v)).replace(/\.?0+$/,"");
    function table(rows, cols) {
      if (!rows || !rows.length) return '<span class="muted">None</span>';
      return '<table><thead><tr>' + cols.map(c=>'<th>'+c[0]+'</th>').join('') + '</tr></thead><tbody>' +
        rows.slice(0,8).map(r=>'<tr>'+cols.map(c=>'<td>'+c[1](r)+'</td>').join('')+'</tr>').join('') + '</tbody></table>';
    }
    async function getJson(path) {
      const res = await fetch(path, { cache: "no-store" });
      if (!res.ok) throw new Error(path + " returned " + res.status);
      return await res.json();
    }
    async function refresh() {
      try {
        const [status, users, peers] = await Promise.all([getJson('/api/exchange/status'), getJson('/api/network/users'), getJson('/api/peers')]);
        $('latest').textContent = status.latest_block_id || '-';
        $('sync').textContent = 'Sync ' + fmt(status.sync_progress) + '% / Genesis ' + (status.genesis_match || '-');
        $('wallet').textContent = fmt(status.wallet_balance) + ' SFR';
        $('walletKey').textContent = short(status.public_key);
        $('peers').textContent = status.local_peers || '0';
        $('peerSync').innerHTML = 'Peer sync <span class="' + (status.peer_sync === 'yes' ? 'ok' : 'bad') + '">' + (status.peer_sync || '-') + '</span>';
        $('supply').textContent = fmt(status.issued_supply) + ' SFR';
        $('ledger').textContent = 'Ledger ' + fmt(status.ledger_balance_total) + ' SFR';
        $('users').innerHTML = table(users.users || [], [['Name', r=>r.name || '-'], ['Balance', r=>fmt(r.balance)], ['Address', r=>short(r.public_key)]]);
        $('peerList').innerHTML = table(peers.peers || [], [['URL', r=>r.url || '-'], ['Reachable', r=>r.reachable || '-'], ['Block', r=>r.latest_block_id || '-'], ['Score', r=>r.score || '-']]);
      } catch (err) {
        $('output').textContent = 'Refresh failed: ' + err.message;
      }
    }
    async function runCommand() {
      const apiKey = $('key').value;
      const command = $('cmd').value.trim();
      if (!command) return;
      sessionStorage.setItem('safire_admin_key', apiKey);
      const body = new URLSearchParams({ api_key: apiKey, command });
      const res = await fetch('/api/admin/command', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body });
      const text = await res.text();
      try { $('output').textContent = JSON.stringify(JSON.parse(text), null, 2); }
      catch (_) { $('output').textContent = text; }
    }
    $('key').value = sessionStorage.getItem('safire_admin_key') || '';
    safeCommands.forEach(c => {
      const button = document.createElement('button');
      button.className = 'chip secondary';
      button.textContent = c;
      button.onclick = () => { $('cmd').value = c; runCommand(); };
      $('chips').appendChild(button);
    });
    refresh();
    setInterval(refresh, 5000);
  </script>
</body>
</html>)HTML";
}

std::string admin_command_json(CBlockDB& block_db, const std::string& command)
{
  std::string normalized = lower_ascii(command);
  std::string payload;
  if (normalized == "status" || normalized == "network" || normalized == "exchange")
  {
    payload = exchange_status_json(block_db);
  }
  else if (normalized == "users")
  {
    payload = network_users_json(block_db);
  }
  else if (normalized == "sync")
  {
    payload = sync_peers_json();
  }
  else if (normalized == "mempool")
  {
    payload = mempool_json();
  }
  else if (normalized == "blockchain" || normalized == "chain")
  {
    payload = recent_blockchain_json(block_db);
  }
  else if (normalized == "peers")
  {
    payload = peers_json(block_db);
  }
  else if (normalized == "accounts")
  {
    payload = wallet_accounts_json(block_db);
  }
  else
  {
    return "{\"status\":\"error\",\"message\":\"Unsupported admin command. Allowed: status, network, sync, users, mempool, blockchain, peers, exchange, accounts.\"}";
  }

  std::stringstream ss;
  ss << "{\"status\":\"ok\",";
  ss << "\"command\":\"" << json_escape(normalized) << "\",";
  ss << "\"output\":" << payload << "}";
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

  if (block_db.getLatestBlockId() < peer.latestBlockId)
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
    return local_peers.empty();
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

  if (request_path == "/admin" || request_path == "/admin/")
  {
    text_reply(rep, reply::ok, admin_page_html(), "text/html");
    return;
  }

  if (request_path == "/api/admin/status")
  {
    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"command_api\":\"" << (configured_admin_api_key().length() > 0 ? "enabled" : "disabled") << "\"}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if ((req.method == "POST" && request_path == "/api/admin/command")
      || request_path.find("/api/admin/command?") == 0)
  {
    if (configured_admin_api_key().length() == 0)
    {
      text_reply(rep, reply::forbidden, "{\"status\":\"error\",\"message\":\"Admin command API is disabled. Add admin_api_key:<secret> to settings.dat to enable it.\"}", "application/json");
      return;
    }
    if (admin_api_authorized(req, request_path) == false)
    {
      text_reply(rep, reply::unauthorized, "{\"status\":\"error\",\"message\":\"Invalid or missing admin API key.\"}", "application/json");
      return;
    }

    std::string command = submitted_value_any(req, request_path, "command");
    if (command.length() == 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Command is required.\"}", "application/json");
      return;
    }

    text_reply(rep, reply::ok, admin_command_json(blockDB, command), "application/json");
    return;
  }

  if (request_path == "/api/status")
  {
    long firstBlockId = blockDB.getFirstBlockId();
    long latestBlockId = blockDB.getLatestBlockId();
    CFunctions::block_structure firstBlock = blockDB.getBlock(firstBlockId);
    CFunctions::block_structure latestBlock = blockDB.getBlock(latestBlockId);
    CNetworkConfig config = CNetworkConfig::load();
    CNatMapper::status natStatus = CNatMapper::currentStatus();
    CWallet wallet;
    std::string privateKey;
    std::string publicKey;
    std::string publicName;
    if (wallet.read(privateKey, publicKey))
    {
      std::map<std::string, std::string> names = accepted_member_names(blockDB);
      publicName = name_for_key(names, publicKey);
    }
    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"protocol_version\":\"" << CLocalPeerClient::PROTOCOL_VERSION << "\",";
    ss << "\"network\":\"" << config.network << "\",";
    ss << "\"public_key\":\"" << json_escape(publicKey) << "\",";
    ss << "\"public_name\":\"" << json_escape(publicName) << "\",";
    ss << "\"first_block_id\":\"" << firstBlockId << "\",";
    ss << "\"first_block_hash\":\"" << firstBlock.hash << "\",";
    ss << "\"expected_genesis_block\":\"" << config.genesisBlock << "\",";
    ss << "\"expected_genesis_hash\":\"" << config.genesisHash << "\",";
    ss << "\"genesis_match\":\"" << (config.genesisMatches(firstBlockId, firstBlock.hash) ? "yes" : "no") << "\",";
    ss << "\"latest_block_id\":\"" << latestBlockId << "\",";
    ss << "\"latest_block_hash\":\"" << latestBlock.hash << "\",";
    ss << "\"public_peer_url\":\"" << json_escape(CLocalPeerClient::getAdvertisedPeer()) << "\",";
    ss << "\"nat_enabled\":\"" << (natStatus.enabled ? "yes" : "no") << "\",";
    ss << "\"nat_mapped\":\"" << (natStatus.mapped ? "yes" : "no") << "\",";
    ss << "\"nat_method\":\"" << json_escape(natStatus.method) << "\",";
    ss << "\"nat_external_address\":\"" << json_escape(natStatus.externalAddress) << "\",";
    ss << "\"nat_external_port\":\"" << natStatus.externalPort << "\",";
    ss << "\"nat_message\":\"" << json_escape(natStatus.message) << "\"}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/schedule/upcoming" || request_path.find("/api/schedule/upcoming?") == 0)
  {
    int limit = 20;
    std::string limitValue = submitted_value_any(req, request_path, "limit");
    if (limitValue.length() > 0)
    {
      limit = std::atoi(limitValue.c_str());
    }
    if (limit <= 0)
    {
      limit = 20;
    }
    if (limit > 100)
    {
      limit = 100;
    }

    std::map<std::string, std::string> names = accepted_member_names(blockDB);
    std::vector<CLocalPeerClient::creator_schedule_slot> schedule = CLocalPeerClient::getUpcomingCreatorSchedule(limit);
    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"epoch_size_blocks\":\"" << CSelector::getEpochSizeBlocks() << "\",";
    ss << "\"selection_lag_epochs\":\"" << CSelector::getSelectionLagEpochs() << "\",";
    ss << "\"slots\":[";
    for (int i = 0; i < schedule.size(); ++i)
    {
      if (i > 0)
      {
        ss << ",";
      }
      CLocalPeerClient::creator_schedule_slot item = schedule.at(i);
      ss << "{";
      ss << "\"block_id\":\"" << item.blockId << "\",";
      ss << "\"creator_key\":\"" << json_escape(item.creatorKey) << "\",";
      ss << "\"creator_name\":\"" << json_escape(name_for_key(names, item.creatorKey)) << "\",";
      ss << "\"creator_peer_url\":\"" << json_escape(item.creatorPeerUrl) << "\",";
      ss << "\"selection_boundary_block\":\"" << item.selectionBoundaryBlock << "\",";
      ss << "\"selection_checkpoint_block\":\"" << item.selectionCheckpointBlock << "\",";
      ss << "\"selection_checkpoint_hash\":\"" << json_escape(item.selectionCheckpointHash) << "\"";
      ss << "}";
    }
    ss << "]}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if (request_path.find("/api/peers/announce") == 0)
  {
    std::string submitted = submitted_value(req, request_path, "url");
    std::string peerUrl = submitted;
    if (submitted.find("\"url\":\"") != std::string::npos)
    {
      peerUrl = json_field(submitted, "url");
    }

    if (peerUrl.length() == 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Missing peer URL.\"}", "application/json");
      return;
    }

    if (CLocalPeerClient::getAdvertisedPeer().length() > 0 &&
        peerUrl.compare(CLocalPeerClient::getAdvertisedPeer()) == 0)
    {
      text_reply(rep, reply::ok, "{\"status\":\"known\",\"message\":\"Peer is this node.\"}", "application/json");
      return;
    }

    std::vector<CLocalPeerClient::peer_status> knownPeers = CLocalPeerClient::getPeerStatuses();
    bool alreadyKnown = false;
    for (int i = 0; i < knownPeers.size(); ++i)
    {
      if (knownPeers.at(i).url.compare(peerUrl) == 0)
      {
        alreadyKnown = true;
        break;
      }
    }

    if (alreadyKnown)
    {
      text_reply(rep, reply::ok, "{\"status\":\"known\",\"message\":\"Peer is already known.\"}", "application/json");
      return;
    }

    if (CLocalPeerClient::addVerifiedPeer(peerUrl, true))
    {
      text_reply(rep, reply::ok, "{\"status\":\"ok\",\"message\":\"Peer announced and verified.\"}", "application/json");
      return;
    }

    text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Peer could not be verified.\"}", "application/json");
    return;
  }

  if (request_path == "/api/peers")
  {
    text_reply(rep, reply::ok, peers_json(blockDB), "application/json");
    return;
  }

  if (request_path == "/api/sync" || (req.method == "POST" && request_path == "/api/sync"))
  {
    text_reply(rep, reply::ok, sync_peers_json(), "application/json");
    return;
  }

  if (request_path == "/api/wallet/accounts")
  {
    text_reply(rep, reply::ok, wallet_accounts_json(blockDB), "application/json");
    return;
  }

  if (request_path.find("/api/wallet/accounts/active?") == 0
      || (req.method == "POST" && request_path == "/api/wallet/accounts/active"))
  {
    CWallet wallet;
    std::string wallet_id = submitted_value_any(req, request_path, "wallet_id");
    if (wallet_id.length() == 0)
    {
      wallet_id = submitted_value_any(req, request_path, "address");
    }
    if (wallet_id.length() == 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"wallet_id is required.\"}", "application/json");
      return;
    }
    if (wallet.setActiveAccount(wallet_id) == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"not_found\",\"message\":\"Wallet account was not found.\"}", "application/json");
      return;
    }
    text_reply(rep, reply::ok, wallet_accounts_json(blockDB), "application/json");
    return;
  }

  if (request_path.find("/api/wallet/accounts/create?") == 0
      || (req.method == "POST" && request_path == "/api/wallet/accounts/create"))
  {
    CWallet wallet;
    CWallet::account created;
    std::string label = submitted_value_any(req, request_path, "label");
    if (wallet.createAccount(label, created) == false)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Unable to create wallet account.\"}", "application/json");
      return;
    }
    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"message\":\"Wallet account created.\",";
    ss << "\"wallet_id\":\"" << json_escape(created.id) << "\",";
    ss << "\"label\":\"" << json_escape(created.label) << "\",";
    ss << "\"public_key\":\"" << json_escape(created.public_key) << "\"}";
    text_reply(rep, reply::created, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/wallet/status" || request_path.find("/api/wallet/status?") == 0)
  {
    CWallet wallet;
    if (wallet.fileExists("wallet.dat") == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\"}", "application/json");
      return;
    }

    std::string privateKey;
    std::string publicKey;
    if (selected_wallet_keys(req, request_path, wallet, privateKey, publicKey) == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"message\":\"Wallet account not found.\"}", "application/json");
      return;
    }

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
    std::vector<CLocalPeerClient::peer_status> localPeers = CLocalPeerClient::getPeerStatuses(true);
    CNatMapper::status natStatus = CNatMapper::currentStatus();
    long peerLatestBlockId = best_peer_latest_block_id(localPeers);
    CLocalPeerClient::peer_status bestPeer;
    bool hasBestPeer = best_peer_status(localPeers, bestPeer);
    bool peerChainMatch = hasBestPeer ? local_chain_matches_peer(blockDB, bestPeer) : true;
    bool peerSync = synced_with_peer_latest(blockDB, latestBlockId, localPeers);
    CLedgerState::state chainState = CLedgerState::build(blockDB, publicKey);
    std::map<std::string, std::string> memberNames = chainState.names;
    double ledgerBalanceTotal = chainState.ledger_balance_total;
    CFunctions pendingFunctions;
    std::vector<CFunctions::record_structure> pendingRecords = pendingFunctions.peekQueueRecords();
    int mempoolRecordCount = pendingRecords.size();
    int pendingWalletRecordCount = 0;
    double pendingWalletDelta = pending_wallet_balance_delta(publicKey, pendingRecords, pendingWalletRecordCount);
    double estimatedWalletBalance = chainState.wallet_balance + pendingWalletDelta;
    double supplyDifference = ledgerBalanceTotal - chainState.issued_supply;
    if (std::fabs(supplyDifference) < 0.000001)
    {
      supplyDifference = 0.0;
    }
    long currentTimeBlock = netTime.getEpoch() / 15;
    long nextTimeBlock = currentTimeBlock + 1;
    std::string currentCreator = "";
    std::string nextCreator = "";
    long currentSelectionBoundary = CSelector::getSelectionBoundaryBlock(currentTimeBlock, firstBlockId);
    long nextSelectionBoundary = CSelector::getSelectionBoundaryBlock(nextTimeBlock, firstBlockId);
    CLedgerState::state currentSelectionState = CLedgerState::build(blockDB, "", currentSelectionBoundary);
    CLedgerState::state nextSelectionState = nextSelectionBoundary == currentSelectionBoundary ?
      currentSelectionState : CLedgerState::build(blockDB, "", nextSelectionBoundary);
    std::vector<std::string> currentActiveMemberKeys = CLedgerState::activeMemberKeysAt(currentSelectionState, currentSelectionState.latest_block_id);
    std::vector<std::string> nextActiveMemberKeys = CLedgerState::activeMemberKeysAt(nextSelectionState, nextSelectionState.latest_block_id);
    if (currentActiveMemberKeys.size() > 0 && currentSelectionState.latest_block.hash.length() > 0)
    {
      currentCreator = CSelector::getSelectedUserForBlock(currentTimeBlock, currentSelectionState.latest_block.hash, currentActiveMemberKeys);
    }
    if (nextActiveMemberKeys.size() > 0 && nextSelectionState.latest_block.hash.length() > 0)
    {
      nextCreator = CSelector::getSelectedUserForBlock(nextTimeBlock, nextSelectionState.latest_block.hash, nextActiveMemberKeys);
    }
    bool walletCreatorEligible = false;
    for (int i = 0; i < currentActiveMemberKeys.size(); ++i)
    {
      if (currentActiveMemberKeys.at(i).compare(publicKey) == 0)
      {
        walletCreatorEligible = true;
        break;
      }
    }
    long creatorEligibilityBlock = -1;
    long creatorEligibilityEtaSeconds = -1;
    if (walletCreatorEligible == false &&
        chainState.joined &&
        chainState.last_heartbeat_block > -1 &&
        firstBlockId > 0)
    {
      long heartbeatEpoch = CSelector::getEpochForBlock(chainState.last_heartbeat_block, firstBlockId);
      long eligibleEpoch = heartbeatEpoch + CSelector::getSelectionLagEpochs();
      creatorEligibilityBlock = firstBlockId + (eligibleEpoch * CSelector::getEpochSizeBlocks());
      creatorEligibilityEtaSeconds = (creatorEligibilityBlock * 15) - netTime.getEpoch();
      if (creatorEligibilityEtaSeconds < 0)
      {
        creatorEligibilityEtaSeconds = 0;
      }
    }
    long secondsUntilNextBlock = (nextTimeBlock * 15) - netTime.getEpoch();
    if (secondsUntilNextBlock < 0)
    {
      secondsUntilNextBlock = 0;
    }

    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"active_wallet_id\":\"" << json_escape(wallet.activeAccountId()) << "\",";
    ss << "\"wallet_id\":\"" << json_escape(publicKey) << "\",";
    ss << "\"public_key\":\"" << publicKey << "\",";
    ss << "\"public_name\":\"" << json_escape(name_for_key(memberNames, publicKey)) << "\",";
    ss << "\"active_member_count\":\"" << currentActiveMemberKeys.size() << "\",";
    ss << "\"epoch_size_blocks\":\"" << CSelector::getEpochSizeBlocks() << "\",";
    ss << "\"selection_lag_epochs\":\"" << CSelector::getSelectionLagEpochs() << "\",";
    ss << "\"selection_boundary_block\":\"" << currentSelectionBoundary << "\",";
    ss << "\"selection_checkpoint_block\":\"" << currentSelectionState.latest_block.number << "\",";
    ss << "\"selection_checkpoint_hash\":\"" << json_escape(currentSelectionState.latest_block.hash) << "\",";
    ss << "\"current_block_id\":\"" << currentTimeBlock << "\",";
    ss << "\"current_block_creator\":\"" << json_escape(currentCreator) << "\",";
    ss << "\"current_block_creator_name\":\"" << json_escape(name_for_key(memberNames, currentCreator)) << "\",";
    ss << "\"current_block_creator_is_wallet\":\"" << (currentCreator.compare(publicKey) == 0 ? "yes" : "no") << "\",";
    ss << "\"next_block_id\":\"" << nextTimeBlock << "\",";
    ss << "\"next_block_creator\":\"" << json_escape(nextCreator) << "\",";
    ss << "\"next_block_creator_name\":\"" << json_escape(name_for_key(memberNames, nextCreator)) << "\",";
    ss << "\"next_block_creator_is_wallet\":\"" << (nextCreator.compare(publicKey) == 0 ? "yes" : "no") << "\",";
    ss << "\"seconds_until_next_block\":\"" << secondsUntilNextBlock << "\",";
    ss << "\"balance\":\"" << chainState.wallet_balance << "\",";
    ss << "\"pending_balance_delta\":\"" << pendingWalletDelta << "\",";
    ss << "\"estimated_balance\":\"" << estimatedWalletBalance << "\",";
    ss << "\"pending_wallet_records\":\"" << pendingWalletRecordCount << "\",";
    ss << "\"mempool_record_count\":\"" << mempoolRecordCount << "\",";
    ss << "\"transaction_fee\":\"" << api_default_transaction_fee() << "\",";
    ss << "\"joined\":\"" << (chainState.joined ? "yes" : "no") << "\",";
    ss << "\"active_heartbeat\":\"" << (chainState.active_heartbeat ? "yes" : "no") << "\",";
    ss << "\"creator_eligible\":\"" << (walletCreatorEligible ? "yes" : "no") << "\",";
    ss << "\"creator_eligibility_boundary_block\":\"" << currentSelectionBoundary << "\",";
    ss << "\"creator_eligibility_checkpoint_block\":\"" << currentSelectionState.latest_block.number << "\",";
    ss << "\"creator_eligibility_eta_block\":\"" << creatorEligibilityBlock << "\",";
    ss << "\"creator_eligibility_eta_seconds\":\"" << creatorEligibilityEtaSeconds << "\",";
    ss << "\"heartbeat_renewal_due\":\"" << (chainState.heartbeat_renewal_due ? "yes" : "no") << "\",";
    ss << "\"last_heartbeat_block\":\"" << chainState.last_heartbeat_block << "\",";
    ss << "\"currency_supply\":\"" << chainState.issued_supply << "\",";
    ss << "\"ledger_balance_total\":\"" << ledgerBalanceTotal << "\",";
    ss << "\"supply_difference\":\"" << supplyDifference << "\",";
    ss << "\"user_count\":\"" << chainState.members.size() << "\",";
    ss << "\"network_up_to_date\":\"" << (latestBlockId > -1 ? "yes" : "no") << "\",";
    ss << "\"peer_sync\":\"" << (peerSync ? "yes" : "no") << "\",";
    ss << "\"peer_latest_block_id\":\"" << peerLatestBlockId << "\",";
    ss << "\"peer_latest_block_hash\":\"" << json_escape(hasBestPeer ? bestPeer.latestBlockHash : "") << "\",";
    ss << "\"peer_chain_match\":\"" << (peerChainMatch ? "yes" : "no") << "\",";
    ss << "\"sync_progress\":\"" << sync_progress_percent(firstBlockId, latestBlockId, localPeers) << "\",";
    ss << "\"first_block_id\":\"" << firstBlockId << "\",";
    ss << "\"latest_block_id\":\"" << latestBlockId << "\",";
    ss << "\"latest_block_hash\":\"" << json_escape(latestBlock.hash) << "\",";
    ss << "\"latest_block_time\":\"" << json_escape(latestBlockTime) << "\",";
    ss << "\"latest_block_record_count\":\"" << latestBlock.records.size() << "\",";
    ss << "\"block_count\":\"" << blockCount << "\",";
    ss << "\"genesis_match\":\"" << (config.genesisMatches(firstBlockId, firstBlock.hash) ? "yes" : "no") << "\",";
    ss << "\"network_time_offset\":\"" << netTime.getOffset() << "\",";
    ss << "\"local_peers\":\"" << localPeers.size() << "\",";
    ss << "\"nat_enabled\":\"" << (natStatus.enabled ? "yes" : "no") << "\",";
    ss << "\"nat_mapped\":\"" << (natStatus.mapped ? "yes" : "no") << "\",";
    ss << "\"nat_method\":\"" << json_escape(natStatus.method) << "\",";
    ss << "\"nat_external_address\":\"" << json_escape(natStatus.externalAddress) << "\",";
    ss << "\"nat_external_port\":\"" << natStatus.externalPort << "\",";
    ss << "\"nat_message\":\"" << json_escape(natStatus.message) << "\"}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/wallet/history" || request_path.find("/api/wallet/history?") == 0)
  {
    CWallet wallet;
    if (wallet.fileExists("wallet.dat") == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"records\":[]}", "application/json");
      return;
    }

    std::string privateKey;
    std::string publicKey;
    if (selected_wallet_keys(req, request_path, wallet, privateKey, publicKey) == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"message\":\"Wallet account not found.\",\"records\":[]}", "application/json");
      return;
    }
    text_reply(rep, reply::ok, wallet_history_json(blockDB, publicKey), "application/json");
    return;
  }

  if (request_path.find("/api/wallet/join?") == 0
      || (req.method == "POST" && request_path == "/api/wallet/join"))
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
    if (selected_wallet_keys(req, request_path, wallet, privateKey, publicKey) == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"message\":\"Wallet account not found.\"}", "application/json");
      return;
    }
    functions.scanChain(publicKey, false);
    if (functions.joined == true)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"This wallet is already joined.\"}", "application/json");
      return;
    }
    if (public_name_available_for_owner(blockDB, name, publicKey) == false)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"That public name is already taken.\"}", "application/json");
      return;
    }

    CFunctions::record_structure joinRecord;
    joinRecord.network = "main";
    CNetworkTime netTime;
    std::stringstream time_stream;
    time_stream << netTime.getEpoch();
    joinRecord.time = time_stream.str();
    joinRecord.transaction_type = CFunctions::JOIN_NETWORK;
    joinRecord.amount = 0.0;
    joinRecord.fee = 0.0;
    joinRecord.sender_public_key = publicKey;
    joinRecord.recipient_public_key = "";
    joinRecord.name = name;
    joinRecord.hash = functions.getRecordHash(joinRecord);

    CECDSACrypto ecdsa;
    std::string signature = "";
    ecdsa.SignMessage(privateKey, joinRecord.hash, signature);
    joinRecord.signature = signature;

    std::string queue_error;
    if (queue_and_broadcast_record(functions, joinRecord, queue_error) == false)
    {
      std::stringstream error;
      error << "{\"status\":\"error\",\"message\":\"Unable to join network: "
            << json_escape(queue_error) << ".\"}";
      text_reply(rep, reply::bad_request, error.str(), "application/json");
      return;
    }

    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"message\":\"Join request queued and broadcast. Joined network will show yes after a block includes this record.\",";
    ss << "\"name\":\"" << json_escape(name) << "\",";
    ss << "\"hash\":\"" << json_escape(joinRecord.hash) << "\"}";
    text_reply(rep, reply::accepted, ss.str(), "application/json");
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
    if (selected_wallet_keys(req, request_path, wallet, privateKey, publicKey) == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"message\":\"Wallet account not found.\"}", "application/json");
      return;
    }
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
    if (selected_wallet_keys(req, request_path, wallet, privateKey, publicKey) == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"message\":\"Wallet account not found.\"}", "application/json");
      return;
    }
    functions.scanChain(publicKey, false);

    double transaction_fee = api_default_transaction_fee();
    std::string fee_value = submitted_value(req, request_path, "fee");
    if (fee_value.length() > 0 && parse_fee_amount(fee_value, transaction_fee) == false)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Fee must be zero or greater.\"}", "application/json");
      return;
    }
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
    sendRecord.nonce = next_transfer_nonce(blockDB, publicKey);
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
    ss << "\"nonce\":\"" << sendRecord.nonce << "\",";
    ss << "\"recipient\":\"" << json_escape(recipient) << "\",";
    ss << "\"hash\":\"" << json_escape(sendRecord.hash) << "\"}";
    text_reply(rep, reply::accepted, ss.str(), "application/json");
    return;
  }

  if (request_path == "/api/exchange/status")
  {
    text_reply(rep, reply::ok, exchange_status_json(blockDB), "application/json");
    return;
  }

  std::string exchangeDepositsPrefix = "/api/exchange/deposits/";
  if (request_path.find(exchangeDepositsPrefix) == 0 || request_path.find("/api/exchange/deposits?") == 0)
  {
    std::string address = "";
    if (request_path.find(exchangeDepositsPrefix) == 0)
    {
      address = path_argument_after_prefix(request_path, exchangeDepositsPrefix);
    }
    if (address.length() == 0)
    {
      address = submitted_value_any(req, request_path, "address");
    }
    if (address.length() == 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Deposit address is required.\",\"deposits\":[]}", "application/json");
      return;
    }
    text_reply(rep, reply::ok, exchange_deposits_json(blockDB, address), "application/json");
    return;
  }

  std::string exchangeTxPrefix = "/api/exchange/tx/";
  if (request_path.find(exchangeTxPrefix) == 0 || request_path.find("/api/exchange/tx?") == 0)
  {
    std::string hash = "";
    if (request_path.find(exchangeTxPrefix) == 0)
    {
      hash = path_argument_after_prefix(request_path, exchangeTxPrefix);
    }
    if (hash.length() == 0)
    {
      hash = submitted_value_any(req, request_path, "hash");
    }
    if (hash.length() == 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Transaction hash is required.\"}", "application/json");
      return;
    }

    exchange_record_location location = find_exchange_record(blockDB, hash);
    if (location.found == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"not_found\",\"message\":\"Transaction was not found.\"}", "application/json");
      return;
    }

    std::map<std::string, std::string> names = accepted_member_names(blockDB);
    std::stringstream ss;
    ss << "{\"status\":\"ok\",\"transaction\":";
    ss << exchange_record_json(location, names);
    ss << "}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  std::string exchangeConfirmationsPrefix = "/api/exchange/confirmations/";
  if (request_path.find(exchangeConfirmationsPrefix) == 0 || request_path.find("/api/exchange/confirmations?") == 0)
  {
    std::string hash = "";
    if (request_path.find(exchangeConfirmationsPrefix) == 0)
    {
      hash = path_argument_after_prefix(request_path, exchangeConfirmationsPrefix);
    }
    if (hash.length() == 0)
    {
      hash = submitted_value_any(req, request_path, "hash");
    }
    if (hash.length() == 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Transaction hash is required.\"}", "application/json");
      return;
    }

    exchange_record_location location = find_exchange_record(blockDB, hash);
    if (location.found == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"not_found\",\"hash\":\"\",\"confirmations\":\"0\"}", "application/json");
      return;
    }

    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"hash\":\"" << json_escape(location.record.hash) << "\",";
    ss << "\"transaction_status\":\"" << (location.pending ? "pending" : "accepted") << "\",";
    ss << "\"block\":\"" << location.block_number << "\",";
    ss << "\"confirmations\":\"" << location.confirmations << "\"}";
    text_reply(rep, reply::ok, ss.str(), "application/json");
    return;
  }

  if (request_path.find("/api/exchange/withdraw?") == 0
      || (req.method == "POST" && request_path == "/api/exchange/withdraw"))
  {
    if (configured_exchange_api_key().length() == 0)
    {
      text_reply(rep, reply::forbidden, "{\"status\":\"error\",\"message\":\"Exchange withdrawal API is disabled. Add exchange_api_key:<secret> to settings.dat to enable it.\"}", "application/json");
      return;
    }
    if (exchange_api_authorized(req, request_path) == false)
    {
      text_reply(rep, reply::unauthorized, "{\"status\":\"error\",\"message\":\"Invalid or missing exchange API key.\"}", "application/json");
      return;
    }

    CWallet wallet;
    if (wallet.fileExists("wallet.dat") == false)
    {
      text_reply(rep, reply::not_found, "{\"status\":\"no_wallet\",\"message\":\"Wallet file not found.\"}", "application/json");
      return;
    }

    std::string recipient = submitted_value_any(req, request_path, "recipient");
    std::string amount_value = submitted_value_any(req, request_path, "amount");
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
    std::string fee_value = submitted_value_any(req, request_path, "fee");
    if (fee_value.length() > 0 && parse_fee_amount(fee_value, transaction_fee) == false)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Fee must be zero or greater.\"}", "application/json");
      return;
    }
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
    sendRecord.nonce = next_transfer_nonce(blockDB, publicKey);
    sendRecord.hash = functions.getRecordHash(sendRecord);

    CECDSACrypto ecdsa;
    std::string signature = "";
    ecdsa.SignMessage(privateKey, sendRecord.hash, signature);
    sendRecord.signature = signature;

    std::string queue_error;
    if (queue_and_broadcast_record(functions, sendRecord, queue_error) == false)
    {
      std::stringstream error;
      error << "{\"status\":\"error\",\"message\":\"Unable to send exchange withdrawal: "
            << json_escape(queue_error) << ".\"}";
      text_reply(rep, reply::bad_request, error.str(), "application/json");
      return;
    }

    std::stringstream ss;
    ss << "{\"status\":\"accepted\",";
    ss << "\"message\":\"Withdrawal queued and broadcast.\",";
    ss << "\"sender\":\"" << json_escape(publicKey) << "\",";
    ss << "\"recipient\":\"" << json_escape(recipient) << "\",";
    ss << "\"amount\":\"" << amount << "\",";
    ss << "\"fee\":\"" << transaction_fee << "\",";
    ss << "\"total\":\"" << total_debit << "\",";
    ss << "\"nonce\":\"" << sendRecord.nonce << "\",";
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

  if (request_path == "/api/handoff/latest")
  {
    text_reply(rep, reply::ok, handoff_latest_json(blockDB), "application/json");
    return;
  }

  if (req.method == "POST" && request_path == "/api/handoff/submit")
  {
    bool accepted = false;
    std::string handoff = submitted_value(req, request_path, "handoff");
    std::string response = submit_handoff_json(blockDB, handoff, accepted);
    text_reply(rep, accepted ? reply::accepted : reply::bad_request, response, "application/json");
    return;
  }

  if (req.method == "POST" && request_path == "/api/blockchain/reset")
  {
    std::string confirmation = submitted_value(req, request_path, "confirm");
    if (confirmation.compare("reset-local-chain") != 0)
    {
      text_reply(rep, reply::bad_request, "{\"status\":\"error\",\"message\":\"Reset confirmation is required.\"}", "application/json");
      return;
    }

    functions.DeleteAll();
    blockDB.DeleteAll();
    std::remove("peers.dat");

    text_reply(rep, reply::ok, "{\"status\":\"ok\",\"message\":\"Local blockchain data was reset. Restart the node to resync from configured peers.\"}", "application/json");
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
    if (block.number <= 0 || block.hash.length() == 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }
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
    if (block.number <= 0 || block.hash.length() == 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }
    text_reply(rep, reply::ok, functions.blockJSON(block), "application/json");
    return;
  }

  if (request_path.find("/api/blocks/submit?") == 0
      || (req.method == "POST" && request_path == "/api/blocks/submit"))
  {
    std::string blockJson = submitted_value(req, request_path, "block");
    std::vector<CFunctions::block_structure> blocks = functions.parseBlockJson(blockJson);
    bool stored = false;
    bool canonical = false;
    bool added = false;
    bool rebroadcast = false;
    bool needsRepair = false;
    std::string message = "invalid block";
    long lastSubmittedBlockId = -1;
    std::string lastSubmittedHash = "";
    for (int i = 0; i < blocks.size(); ++i)
    {
      CFunctions::block_structure block = blocks.at(i);
      if (block.number > 0)
      {
        long latestBefore = blockDB.getLatestBlockId();
        bool alreadyStored = blockDB.getBlockByHash(block.hash).number > 0;
        bool blockAdded = blockDB.AddBlock(block);
        bool blockStored = alreadyStored || blockAdded;
        if (alreadyStored == false && blockAdded && blockDB.getFirstBlockId() == -1 && block.previous_block_id <= 0)
        {
          blockDB.setFirstBlockId(block.number);
        }
        long latestAfter = blockDB.getLatestBlockId();
        if (alreadyStored == false && blockAdded && latestAfter < block.number)
        {
          needsRepair = true;
        }
        CFunctions::block_structure canonicalBlock = blockDB.getBlock(block.number);
        bool blockCanonical = canonicalBlock.number == block.number &&
          canonicalBlock.hash.compare(block.hash) == 0 &&
          latestAfter >= block.number;

        if (blockStored)
        {
          stored = true;
          message = blockCanonical ? "block accepted into canonical chain" : "block stored but not connected";
          lastSubmittedBlockId = block.number;
          lastSubmittedHash = block.hash;
        }
        if (blockCanonical)
        {
          canonical = true;
        }
        added = (alreadyStored == false && blockAdded) || added;

        CNetworkTime netTime;
        long currentSlot = netTime.getEpoch() / 15;
        bool recentBlock = block.number + 120 >= currentSlot;
        bool advancedTip = latestAfter >= block.number && latestAfter >= latestBefore;
        if (alreadyStored == false && blockAdded && blockCanonical && advancedTip && recentBlock)
        {
          CLocalPeerClient::broadcastBlock(block);
          rebroadcast = true;
        }
      }
    }

    if (needsRepair)
    {
      blockDB.rebuildBestChainIndex();
    }

    long latestBlockId = blockDB.getLatestBlockId();
    std::string status = canonical ? "accepted" : (stored ? "stored" : "invalid");
    std::stringstream ss;
    ss << "{\"status\":\"" << status << "\",";
    ss << "\"stored\":\"" << (stored ? "yes" : "no") << "\",";
    ss << "\"canonical\":\"" << (canonical ? "yes" : "no") << "\",";
    ss << "\"rebroadcast\":\"" << (rebroadcast ? "yes" : "no") << "\",";
    ss << "\"block\":\"" << lastSubmittedBlockId << "\",";
    ss << "\"hash\":\"" << json_escape(lastSubmittedHash) << "\",";
    ss << "\"latest_block_id\":\"" << latestBlockId << "\",";
    ss << "\"message\":\"" << json_escape(message) << "\"}";
    text_reply(rep, stored ? reply::accepted : reply::bad_request, ss.str(), "application/json");
    return;
  }

  std::string blockBatchAfterHashPrefix = "/api/blocks/batch-after-hash/";
  if (request_path.find(blockBatchAfterHashPrefix) == 0)
  {
    std::string blockHashAndLimit = request_path.substr(blockBatchAfterHashPrefix.length());
    std::string blockHash = blockHashAndLimit;
    int limit = 100;
    std::size_t separator = blockHashAndLimit.find("/");
    if (separator != std::string::npos)
    {
      blockHash = blockHashAndLimit.substr(0, separator);
      limit = ::atoi(blockHashAndLimit.substr(separator + 1).c_str());
    }
    if (limit <= 0)
    {
      limit = 1;
    }
    if (limit > 250)
    {
      limit = 250;
    }

    CFunctions::block_structure block = blockDB.getBlockByHash(blockHash);
    if (block.number <= 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }

    std::stringstream batch;
    int count = 0;
    while (count < limit)
    {
      CFunctions::block_structure nextBlock = blockDB.getNextBlockByHash(block);
      if (nextBlock.number <= 0 || nextBlock.number == block.number)
      {
        break;
      }
      batch << functions.blockJSON(nextBlock);
      block = nextBlock;
      ++count;
    }

    if (count == 0)
    {
      text_reply(rep, reply::not_found, "", "text/plain");
      return;
    }
    text_reply(rep, reply::ok, batch.str(), "application/json");
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
