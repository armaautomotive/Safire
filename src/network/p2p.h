/**
* CP2P
*
* Description: P2P interface. Uses libnice to connect through a stun server.
*/
#ifndef P2P_H
#define P2P_H


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
//#include <vector>

//#include <stdlib.h>
//#include <stdio.h>
//#include <limits.h>
//#include <string.h>
//#include <assert.h>

//define CHUNK_SIZE 1024
static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
static const gchar *state_name[] = {"disconnected", "gathering", "connecting", "connected", "ready", "failed"};

static int print_local_data(NiceAgent *agent, guint stream_id, guint component_id);
static int parse_remote_data(NiceAgent *agent, guint stream_id, guint component_id, char *line);
static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data);
static void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id, gchar *lfoundation, gchar *rfoundation, gpointer data);
static void cb_component_state_changed(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer data);
static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data);
static gboolean stdin_remote_info_cb (GIOChannel *source, GIOCondition cond, gpointer data);
static gboolean stdin_send_data_cb (GIOChannel *source, GIOCondition cond, gpointer data);

static std::string get_local_data (NiceAgent *agent, guint _stream_id, guint component_id);

static GMainLoop *gloop;
static GIOChannel* io_stdin;
static guint stream_id;

//NiceAgent *agent;

class CP2P
{
private:
    NiceAgent *agent;

    //static GMainLoop *gloop;
    //static GIOChannel* io_stdin;
    //static guint stream_id;

    gchar *stun_addr = NULL;
    guint stun_port = 0; 
    gboolean controlling;

public:

    CP2P();

    //! Destructor.
    ~CP2P()
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
    void p2pNetworkThread(int argc, char* argv[]);

};

#ifdef __cplusplus
extern "C" {
#endif

typedef CP2P * CP2PHandle;
CP2PHandle create_cp2p();
void free_cp2p(CP2PHandle);
void setPeerAddress_cp2p(CP2PHandle, std::string address);

typedef struct CP2P CP2P; // C reference to Class.
extern "C" void MyClass_function(CP2P *obj, std::string var);
//EXPORT_C CP2P_class* CP2P_new(void);

std::string getNewNetworkPeer_cp2p(CP2PHandle, std::string myPeerAddress);


#ifdef __cplusplus
}
#endif


#endif // P2P_H
