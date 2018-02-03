/**
* CRelayClient
*
* Description: Server network relay interface. Uses libnice to connect through a stun server.
*/
#ifndef RELAY_CLIENT_H
#define RELAY_CLIENT_H

#include "functions/functions.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <agent.h>

#include <gio/gnetworking.h>

#include <thread>
#include <iostream>
#include <string>
#include <unistd.h>             // sleep
#include <curl/curl.h>

//#include <stdexcept>
#include <vector>
//#include <stdlib.h>
//#include <stdio.h>
//#include <limits.h>
//#include <string.h>
//#include <assert.h>
//static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
//static const gchar *state_name[] = {"disconnected", "gathering", "connecting", "connected", "ready", "failed"};

static int print_local_data(NiceAgent *agent, guint stream_id, guint component_id);
static int parse_remote_data(NiceAgent *agent, guint stream_id, guint component_id, char *line);
static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data);
static void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id, gchar *lfoundation, gchar *rfoundation, gpointer data);
static void cb_component_state_changed(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer data);
static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data);
static gboolean stdin_remote_info_cb (GIOChannel *source, GIOCondition cond, gpointer data);
static gboolean stdin_send_data_cb (GIOChannel *source, GIOCondition cond, gpointer data);
static std::string get_local_data (NiceAgent *agent, guint _stream_id, guint component_id);

class CRelayClient
{
public:
    struct node_status { 
        std::string public_key;
        bool active = true;
    };
private:
    static std::vector< CRelayClient::node_status > node_statuses; // TODO: rename node_peers 
    
    gchar *stun_addr = NULL;
    guint stun_port = 0; 
    gboolean controlling;

public:
    
    CRelayClient();
    
    //! Destructor.
    ~CRelayClient()
    {
    }
    
    static bool running;
    static bool connected;
    
    static std::string myPeerAddress;
    static std::string remotePeerAddress;
    
    void setMyPeerAddress(std::string address);
    std::string getNewNetworkPeer(std::string myPeerAddress);
    void connect();
    void exit();
    void sendData(std::string data);
    //void cb_candidate_gathering_done_X(NiceAgent *agent, guint stream_id, gpointer data);
    void relayNetworkThread(int argc, char* argv[]);

    void sendRecord(CFunctions::record_structure record);
    void sendBlock(CFunctions::block_structure block);    

    void receiveRecords();
    void receiveBlocks();

    std::vector<CRelayClient::node_status> getPeers();
};

#endif 
