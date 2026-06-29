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
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>
#include "mime_types.h"
#include "reply.h"
#include "request.h"
#include "wallet.h"
#include "blockdb.h"
#include "functions/functions.h"
#include "network/localpeerclient.h"
#include "networkconfig.h"

#include "networktime.h"

namespace http {
namespace server3 {

namespace {

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

std::string normalized_public_name(const std::string& name)
{
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

std::string wallet_history_json(CBlockDB& block_db, const std::string& public_key)
{
  long first_block_id = block_db.getFirstBlockId();
  long latest_block_id = block_db.getLatestBlockId();
  if (first_block_id < 0 || latest_block_id < 0)
  {
    return "{\"status\":\"ok\",\"records\":[]}";
  }

  const int limit = 25;
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
    CNetworkConfig config = CNetworkConfig::load();
    CFunctions::block_structure firstBlock = blockDB.getBlock(firstBlockId);
    CNetworkTime netTime;
    std::vector<CLocalPeerClient::peer_status> localPeers = CLocalPeerClient::getPeerStatuses();

    std::stringstream ss;
    ss << "{\"status\":\"ok\",";
    ss << "\"public_key\":\"" << publicKey << "\",";
    ss << "\"balance\":\"" << functions.balance << "\",";
    ss << "\"joined\":\"" << (functions.joined ? "yes" : "no") << "\",";
    ss << "\"active_heartbeat\":\"" << (functions.active_heartbeat ? "yes" : "no") << "\",";
    ss << "\"heartbeat_renewal_due\":\"" << (functions.heartbeat_renewal_due ? "yes" : "no") << "\",";
    ss << "\"last_heartbeat_block\":\"" << functions.last_heartbeat_block << "\",";
    ss << "\"currency_supply\":\"" << functions.currency_circulation << "\",";
    ss << "\"user_count\":\"" << functions.user_count << "\",";
    ss << "\"network_up_to_date\":\"" << (functions.IsChainUpToDate() ? "yes" : "no") << "\",";
    ss << "\"sync_progress\":\"" << sync_progress_percent(firstBlockId, latestBlockId, localPeers) << "\",";
    ss << "\"first_block_id\":\"" << firstBlockId << "\",";
    ss << "\"latest_block_id\":\"" << latestBlockId << "\",";
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
